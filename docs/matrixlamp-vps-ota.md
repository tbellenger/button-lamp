# Matrix lamp OTA file on the VPS

The lamp compares versions against `manifest.json` on the GitHub release `matrixlamp-latest`. When that version is newer, the ESP32 bridge downloads:

`http://api.drunkwallaby.com/ota/matrix/latest.ota`

That URL has to answer **200** with a **Content-Length** and **no query string**. Bridge firmware 0.6.0 (`Arduino_ESP32_OTA` 0.3.1) calls `HttpClient::get(path())` and drops the query, so a signed `release-assets.githubusercontent.com` URL cannot be installed. Plain HTTP also skips the release-asset certificate.

The release workflow copies two files after it publishes on `main` (not on a pull request):

| Public URL | File on the VPS |
|---|---|
| `http://api.drunkwallaby.com/ota/matrix/latest.ota` | `/var/www/ota/matrix/latest.ota` |
| `http://api.drunkwallaby.com/ota/matrix/latest.json` | `/var/www/ota/matrix/latest.json` |

`latest.json` is the same compact manifest as the GitHub release. The lamp does not fetch it. The version check stays on GitHub so a missed copy cannot make Install update report "up to date" against a stale file while the release is already newer. The JSON is there for inspection, and so a later firmware can switch the check to plain HTTP without another server change.

The workflow uploads `latest.ota.partial` and `latest.json.partial`, then renames them into place. It uses repo secrets `VPS_HOST`, `VPS_USER`, `VPS_SSH_KEY`, and `VPS_KNOWN_HOSTS`. If any of those is empty it prints a warning and the job still succeeds. It does not disable host-key checking.

## One-time setup

Run these on the VPS as root. They do not change Caddy until the snippet below is added by hand.

```sh
useradd --system --create-home --shell /usr/sbin/nologin matrixota
install -d -o matrixota -g caddy -m 755 /var/www/ota /var/www/ota/matrix
```

`caddy` is the user the Caddy service runs as. If `id caddy` fails, use the user from `ps -o user= -C caddy`. Parents of `/var/www/ota` must be traversable by that user (`755` is enough). The firmware is public, so the files can be mode `644`.

Deploy key, on a machine that will not be the VPS:

```sh
ssh-keygen -t ed25519 -f matrixota_deploy -N "" -C "github-actions matrix ota"
```

`matrixota_deploy` is private. Put that file, including the `BEGIN` and `END` lines, in the GitHub secret `VPS_SSH_KEY`. `matrixota_deploy.pub` is the public half.

Host key, from any machine that can reach the VPS:

```sh
ssh-keyscan -t ed25519 api.drunkwallaby.com
```

Put that one line in the GitHub secret `VPS_KNOWN_HOSTS`. Also set `VPS_HOST` to `api.drunkwallaby.com` and `VPS_USER` to `matrixota`.

Receiver, on the VPS, mode `755`, owner `root`:

```sh
cat >/usr/local/sbin/matrix-ota-recv <<'EOF'
#!/bin/sh
set -eu
case "$SSH_ORIGINAL_COMMAND" in
  "scp -t /var/www/ota/matrix/latest.ota.partial"|"scp -t /var/www/ota/matrix/latest.json.partial")
    exec $SSH_ORIGINAL_COMMAND
    ;;
  "mv -f /var/www/ota/matrix/latest.ota.partial /var/www/ota/matrix/latest.ota && mv -f /var/www/ota/matrix/latest.json.partial /var/www/ota/matrix/latest.json")
    exec sh -c "$SSH_ORIGINAL_COMMAND"
    ;;
  *)
    echo "denied" >&2
    exit 1
    ;;
esac
EOF
chmod 755 /usr/local/sbin/matrix-ota-recv
install -d -o matrixota -g matrixota -m 700 /home/matrixota/.ssh
```

`authorized_keys` for `matrixota`, one line. `restrict` turns off port forwarding, X11, agent forwarding, and a TTY. The command is the only thing the key can run, and the script only accepts the two `scp -t` writes and the rename the workflow uses (`scp -O` on the runner, so the remote command is legacy `scp -t`, not SFTP):

```text
command="/usr/local/sbin/matrix-ota-recv",restrict ssh-ed25519 AAAA... github-actions matrix ota
```

Paste the contents of `matrixota_deploy.pub` after `restrict `.

## Caddy

This is an addition to the existing `http://api.drunkwallaby.com` site that already serves `/radar.bin` on port 80 and redirects every other path to HTTPS. Handle `/ota/*` before that redirect. Do not add an `encode` directive: gzip drops `Content-Length`, and the bridge rejects chunked responses. `file_server` sends `Content-Length`. There is no query string.

```caddy
http://api.drunkwallaby.com {
	@radar path /radar.bin /radar.json
	handle @radar {
		reverse_proxy 127.0.0.1:8091
		header {
			-Server
			-Alt-Svc
			-Via
			-X-Request-Id
			-Transfer-Encoding
		}
	}

	@ota path /ota/*
	handle @ota {
		root * /var/www
		file_server
		header {
			-Server
			-Alt-Svc
			-Via
			-X-Request-Id
			-Transfer-Encoding
		}
	}

	handle {
		redir https://{host}{uri} permanent
	}
}
```

`/ota/matrix/latest.ota` is then the file `/var/www/ota/matrix/latest.ota`. Reload Caddy after editing. A request for that URL before the first release copy should be a 404 with no redirect to HTTPS.

## Check

After the secrets exist and a `main` release has run:

```sh
curl -sI http://api.drunkwallaby.com/ota/matrix/latest.ota
curl -sI http://api.drunkwallaby.com/ota/matrix/latest.json
```

Both should be `HTTP/1.1 200` with `Content-Length` set and no `Transfer-Encoding: chunked`. `latest.ota` begins with the OTA magic `02 10 41 23` (little-endian `0x23411002`). `latest.json` `version` matches the `matrixlamp-latest` GitHub release.
