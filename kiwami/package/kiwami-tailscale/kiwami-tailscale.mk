################################################################################
#
# kiwami-tailscale
#
################################################################################

# 1.98.8 is the newest that builds with Buildroot's Go (1.26.3): a newer one's go.mod asks for a newer Go
KIWAMI_TAILSCALE_VERSION = 1.98.8
KIWAMI_TAILSCALE_SITE = $(call github,tailscale,tailscale,v$(KIWAMI_TAILSCALE_VERSION))
KIWAMI_TAILSCALE_LICENSE = BSD-3-Clause
KIWAMI_TAILSCALE_LICENSE_FILES = LICENSE
KIWAMI_TAILSCALE_GOMOD = tailscale.com
KIWAMI_TAILSCALE_BUILD_TARGETS = cmd/tailscaled
KIWAMI_TAILSCALE_LDFLAGS = \
	-X tailscale.com/version.longStamp=$(KIWAMI_TAILSCALE_VERSION) \
	-X tailscale.com/version.shortStamp=$(KIWAMI_TAILSCALE_VERSION)
# Vendored through Go's module proxy, not from each module's repository (Buildroot's GOPROXY=direct): one of
# Tailscale's dependencies, github.com/kenshaw/evdev v0.1.0, is no longer on GitHub, and the proxy keeps it. go.sum
# checks every module whichever way it comes; the build itself is offline (-mod=vendor).
KIWAMI_TAILSCALE_GO_ENV = GOPROXY=https://proxy.golang.org,direct

# the CLI inside tailscaled (one Go runtime on the card, not two)
KIWAMI_TAILSCALE_TAGS = ts_include_cli
# Features left out (Tailscale's own list: feature/featuretags/featuretags.go). Kept: the port mapper, DERP and
# netcheck (always in), osrouter (kernel mode's addresses and routes), the IPN bus (`tailscale up` waits on it for the
# login), health (the page's errors), DNS, Tailnet Lock, c2n, the peer API, the baked-in CA roots, debug and doctor.
# what the investigation leaves out
KIWAMI_TAILSCALE_TAGS += \
	ts_omit_ssh \
	ts_omit_taildrop \
	ts_omit_serve \
	ts_omit_webclient \
	ts_omit_acme \
	ts_omit_drive \
	ts_omit_advertiseexitnode \
	ts_omit_useexitnode \
	ts_omit_advertiseroutes \
	ts_omit_useroutes \
	ts_omit_appconnectors \
	ts_omit_conn25 \
	ts_omit_relayserver \
	ts_omit_tap \
	ts_omit_capture \
	ts_omit_wakeonlan
# userspace networking (gVisor) and what needs it: kernel mode uses the tun module
KIWAMI_TAILSCALE_TAGS += \
	ts_omit_netstack \
	ts_omit_gro \
	ts_omit_outboundproxy
# what this OS doesn't have: netfilter, systemd, D-Bus, NetworkManager, a desktop, a browser, a cloud, a TPM
KIWAMI_TAILSCALE_TAGS += \
	ts_omit_iptables \
	ts_omit_sdnotify \
	ts_omit_dbus \
	ts_omit_resolved \
	ts_omit_networkmanager \
	ts_omit_linuxdnsfight \
	ts_omit_desktop_sessions \
	ts_omit_systray \
	ts_omit_webbrowser \
	ts_omit_aws \
	ts_omit_cloud \
	ts_omit_kube \
	ts_omit_bird \
	ts_omit_synology \
	ts_omit_tpm
# logs not uploaded (--no-logs-no-support), Tailscale updated with the image, no MDM, auth keys only as keys
KIWAMI_TAILSCALE_TAGS += \
	ts_omit_logtail \
	ts_omit_netlog \
	ts_omit_clientupdate \
	ts_omit_syspolicy \
	ts_omit_posture \
	ts_omit_oauthkey \
	ts_omit_identityfederation \
	ts_omit_hujsonconf
# what polls in the background for something nobody looks at: listening ports, the tun's counters and link speed
KIWAMI_TAILSCALE_TAGS += \
	ts_omit_portlist \
	ts_omit_tundevstats \
	ts_omit_linkspeed \
	ts_omit_usermetrics
# the CLI's terminal niceties (the menu draws its own QR) and developers' debugging; one user, root
KIWAMI_TAILSCALE_TAGS += \
	ts_omit_qrcodes \
	ts_omit_colorable \
	ts_omit_completion \
	ts_omit_completion_scripts \
	ts_omit_debugeventbus \
	ts_omit_debugportmapper \
	ts_omit_unixsocketidentity

# the CLI by its name: tailscaled runs as the CLI when called `tailscale`
define KIWAMI_TAILSCALE_INSTALL_CLI
	ln -sf tailscaled $(TARGET_DIR)/usr/bin/tailscale
endef
KIWAMI_TAILSCALE_POST_INSTALL_TARGET_HOOKS += KIWAMI_TAILSCALE_INSTALL_CLI

$(eval $(golang-package))
