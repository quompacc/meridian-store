//! MeridianOS Store — privileged transaction daemon.
//!
//! Owns `org.meridian.Store` on the **system bus**. The unprivileged GUI calls
//! it over D-Bus; every mutating action is gated by polkit. Iteration 1 uses
//! `pacman` as the transaction engine (the reference implementation) and streams
//! its output as `Progress` signals — the engine is deliberately swappable for a
//! native libalpm transaction once that path is validated in a VM.
//!
//! Security posture:
//!   * polkit check (`org.meridian.store.manage`) before any install/remove/update,
//!   * strict package-name validation (no flag/shell injection; `--` separator),
//!   * single in-flight transaction (a busy lock).

use std::collections::HashMap;
use std::process::Stdio;
use std::sync::Arc;

use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::process::Command;
use tokio::sync::Mutex;

use zbus::interface;
use zbus::object_server::SignalContext;
use zbus_polkit::policykit1::{AuthorityProxy, CheckAuthorizationFlags, Subject};

const PACMAN: &str = "/usr/bin/pacman";
const BUILD_AUR: &str = "/usr/lib/meridian-store/build-aur.sh";
const POLKIT_ACTION: &str = "org.meridian.store.manage";

struct Store {
    /// Held for the duration of a transaction; rejects concurrent ones.
    busy: Arc<Mutex<()>>,
}

/// Accept only well-formed pacman package names. Rejects anything that could be
/// read as a flag (leading `-`) or smuggle shell/path characters.
fn valid_pkg_name(name: &str) -> bool {
    !name.is_empty()
        && name.len() <= 256
        && !name.starts_with('-')
        && name
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || matches!(c, '@' | '.' | '_' | '+' | '-'))
}

/// Accept only a git commit id (short or full hex). The GUI pins the AUR build
/// to the exact revision the user reviewed; the daemon enforces the shape.
fn valid_rev(rev: &str) -> bool {
    (7..=40).contains(&rev.len()) && rev.chars().all(|c| c.is_ascii_hexdigit())
}

impl Store {
    /// Ask polkit whether `sender` may perform a managing action.
    async fn check_auth(
        &self,
        connection: &zbus::Connection,
        header: &zbus::message::Header<'_>,
    ) -> zbus::fdo::Result<()> {
        let subject = Subject::new_for_message_header(header)
            .map_err(|e| zbus::fdo::Error::Failed(format!("polkit subject: {e}")))?;
        let authority = AuthorityProxy::new(connection)
            .await
            .map_err(|e| zbus::fdo::Error::Failed(format!("polkit proxy: {e}")))?;
        let details: HashMap<&str, &str> = HashMap::new();
        let result = authority
            .check_authorization(
                &subject,
                POLKIT_ACTION,
                &details,
                CheckAuthorizationFlags::AllowUserInteraction.into(),
                "",
            )
            .await
            .map_err(|e| zbus::fdo::Error::Failed(format!("polkit check: {e}")))?;
        if result.is_authorized {
            Ok(())
        } else {
            Err(zbus::fdo::Error::AccessDenied(
                "Nicht autorisiert (polkit)".into(),
            ))
        }
    }

    /// Run a program with a fixed argv, streaming stdout+stderr as Progress
    /// signals. Returns the finished status; also emits the `finished` signal.
    async fn run_command(
        ctxt: &SignalContext<'_>,
        program: &str,
        args: Vec<String>,
    ) -> (bool, String) {
        let owned = ctxt.to_owned();
        let mut child = match Command::new(program)
            .args(&args)
            .env("LC_ALL", "C.UTF-8")
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
        {
            Ok(c) => c,
            Err(e) => {
                let msg = format!("{program} konnte nicht gestartet werden: {e}");
                let _ = Store::finished(ctxt, false, &msg).await;
                return (false, msg);
            }
        };

        let stdout = child.stdout.take().expect("stdout piped");
        let stderr = child.stderr.take().expect("stderr piped");

        // Drain stderr in the background, mirroring lines to Progress.
        let err_ctxt = owned.clone();
        let err_task = tokio::spawn(async move {
            let mut buf = String::new();
            let mut lines = BufReader::new(stderr).lines();
            while let Ok(Some(line)) = lines.next_line().await {
                let _ = Store::progress(&err_ctxt, &line).await;
                buf.push_str(&line);
                buf.push('\n');
            }
            buf
        });

        let mut lines = BufReader::new(stdout).lines();
        while let Ok(Some(line)) = lines.next_line().await {
            let _ = Store::progress(ctxt, &line).await;
        }

        let status = child.wait().await;
        let errbuf = err_task.await.unwrap_or_default();
        let success = matches!(&status, Ok(s) if s.success());
        let message = if success {
            "Fertig".to_string()
        } else {
            let tail: String = errbuf.lines().rev().take(4).collect::<Vec<_>>().join("\n");
            if tail.is_empty() {
                "Befehl beendete sich mit Fehler".to_string()
            } else {
                tail
            }
        };
        let _ = Store::finished(ctxt, success, &message).await;
        (success, message)
    }
}

#[interface(name = "org.meridian.Store1")]
impl Store {
    /// Refresh sync databases and upgrade in one step (`pacman -Syu`).
    ///
    /// Deliberately a full `-Syu`, never a lone `-Sy`: syncing the databases
    /// without upgrading is the classic Arch partial-upgrade footgun — a later
    /// single-package install would pull newer dependencies against an
    /// un-upgraded system. Refreshing always implies upgrading.
    async fn refresh(
        &self,
        #[zbus(connection)] connection: &zbus::Connection,
        #[zbus(header)] header: zbus::message::Header<'_>,
        #[zbus(signal_context)] ctxt: SignalContext<'_>,
    ) -> zbus::fdo::Result<bool> {
        self.check_auth(connection, &header).await?;
        let _guard = self
            .busy
            .try_lock()
            .map_err(|_| zbus::fdo::Error::Failed("Transaktion läuft bereits".into()))?;
        let (ok, _) =
            Store::run_command(&ctxt, PACMAN, vec!["-Syu".into(), "--noconfirm".into()]).await;
        Ok(ok)
    }

    /// Install the named packages (`pacman -S --noconfirm -- <names>`).
    async fn install(
        &self,
        #[zbus(connection)] connection: &zbus::Connection,
        #[zbus(header)] header: zbus::message::Header<'_>,
        #[zbus(signal_context)] ctxt: SignalContext<'_>,
        packages: Vec<String>,
    ) -> zbus::fdo::Result<bool> {
        if packages.is_empty() || !packages.iter().all(|p| valid_pkg_name(p)) {
            return Err(zbus::fdo::Error::InvalidArgs(
                "Ungültiger Paketname".into(),
            ));
        }
        self.check_auth(connection, &header).await?;
        let _guard = self
            .busy
            .try_lock()
            .map_err(|_| zbus::fdo::Error::Failed("Transaktion läuft bereits".into()))?;
        let mut args = vec!["-S".to_string(), "--noconfirm".to_string(), "--".to_string()];
        args.extend(packages);
        let (ok, _) = Store::run_command(&ctxt, PACMAN, args).await;
        Ok(ok)
    }

    /// Remove the named packages with unneeded deps (`pacman -Rs --noconfirm -- <names>`).
    async fn remove(
        &self,
        #[zbus(connection)] connection: &zbus::Connection,
        #[zbus(header)] header: zbus::message::Header<'_>,
        #[zbus(signal_context)] ctxt: SignalContext<'_>,
        packages: Vec<String>,
    ) -> zbus::fdo::Result<bool> {
        if packages.is_empty() || !packages.iter().all(|p| valid_pkg_name(p)) {
            return Err(zbus::fdo::Error::InvalidArgs(
                "Ungültiger Paketname".into(),
            ));
        }
        self.check_auth(connection, &header).await?;
        let _guard = self
            .busy
            .try_lock()
            .map_err(|_| zbus::fdo::Error::Failed("Transaktion läuft bereits".into()))?;
        let mut args = vec!["-Rs".to_string(), "--noconfirm".to_string(), "--".to_string()];
        args.extend(packages);
        let (ok, _) = Store::run_command(&ctxt, PACMAN, args).await;
        Ok(ok)
    }

    /// Full system upgrade (`pacman -Syu --noconfirm`).
    async fn update_system(
        &self,
        #[zbus(connection)] connection: &zbus::Connection,
        #[zbus(header)] header: zbus::message::Header<'_>,
        #[zbus(signal_context)] ctxt: SignalContext<'_>,
    ) -> zbus::fdo::Result<bool> {
        self.check_auth(connection, &header).await?;
        let _guard = self
            .busy
            .try_lock()
            .map_err(|_| zbus::fdo::Error::Failed("Transaktion läuft bereits".into()))?;
        let (ok, _) =
            Store::run_command(&ctxt, PACMAN, vec!["-Syu".into(), "--noconfirm".into()]).await;
        Ok(ok)
    }

    /// Build and install an AUR package by pkgbase, pinned to the exact commit
    /// `rev` the user reviewed in the GUI. Clones the AUR repo, checks out that
    /// revision (aborting if it no longer exists — i.e. the repo was rewritten),
    /// and builds in a clean chroot (devtools) — never on the host — then
    /// installs the result.
    ///
    /// The `rev` pin closes a TOCTOU: without it the daemon would build whatever
    /// is at HEAD *now*, not the bytes the user actually read and approved.
    async fn build_aur(
        &self,
        #[zbus(connection)] connection: &zbus::Connection,
        #[zbus(header)] header: zbus::message::Header<'_>,
        #[zbus(signal_context)] ctxt: SignalContext<'_>,
        pkg_base: String,
        rev: String,
    ) -> zbus::fdo::Result<bool> {
        if !valid_pkg_name(&pkg_base) {
            return Err(zbus::fdo::Error::InvalidArgs("Ungültiger Paketname".into()));
        }
        if !valid_rev(&rev) {
            return Err(zbus::fdo::Error::InvalidArgs("Ungültige Revision".into()));
        }
        self.check_auth(connection, &header).await?;
        let _guard = self
            .busy
            .try_lock()
            .map_err(|_| zbus::fdo::Error::Failed("Transaktion läuft bereits".into()))?;
        let (ok, _) = Store::run_command(&ctxt, BUILD_AUR, vec![pkg_base, rev]).await;
        Ok(ok)
    }

    /// List available updates as (name, old_version, new_version). Read-only, no
    /// polkit gate. Empty result also when fully up to date.
    async fn list_updates(&self) -> Vec<(String, String, String)> {
        let out = match Command::new(PACMAN)
            .arg("-Qu")
            .env("LC_ALL", "C.UTF-8")
            .stdin(Stdio::null())
            .output()
            .await
        {
            Ok(o) => o,
            Err(_) => return Vec::new(),
        };
        let text = String::from_utf8_lossy(&out.stdout);
        let mut updates = Vec::new();
        for line in text.lines() {
            // "name oldver -> newver"  (may carry a trailing "[ignored]")
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.len() >= 4 && parts[2] == "->" {
                updates.push((
                    parts[0].to_string(),
                    parts[1].to_string(),
                    parts[3].to_string(),
                ));
            }
        }
        updates
    }

    #[zbus(signal)]
    async fn progress(ctxt: &SignalContext<'_>, line: &str) -> zbus::Result<()>;

    #[zbus(signal)]
    async fn finished(ctxt: &SignalContext<'_>, success: bool, message: &str) -> zbus::Result<()>;
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let store = Store {
        busy: Arc::new(Mutex::new(())),
    };

    let _conn = zbus::connection::Builder::system()?
        .name("org.meridian.Store")?
        .serve_at("/org/meridian/Store", store)?
        .build()
        .await?;

    // Serve until killed (D-Bus / systemd manages our lifetime).
    std::future::pending::<()>().await;
    Ok(())
}
