#pragma once
// banana-ui (kiwami): Tailscale's state, from the helper's `status` (kiwami/rootfs/usr/libexec/kiwami/tailscale):
// "state off" or "state starting" while there is no tailscaled to ask, "error <text>" for the last failure the helper
// knows, then `tailscale status --json` (its fields as tailscaled 1.98.8 writes them). And the device's name in the
// tailnet, its login link as a QR code, and the devices page's list. Pure functions, no I/O.
#include <string>
#include <vector>

namespace banana::kiwami
{
	struct TsDevice
	{
		std::string name;		// the name in the tailnet: the MagicDNS name's first label, else the host name
		std::string hostName, dnsName, os;
		std::string ipv4, ipv6;	// its first of each, "" for none
		std::string lastSeen;	// RFC 3339, "" when never (or online)
		bool online = false, expired = false;
		bool self = false;		// this handheld (deviceList)
	};

	struct TsStatus
	{
		// tailscaled's BackendState: NoState and Starting are STARTING, NeedsMachineAuth NEEDS_APPROVAL; OFF is the
		// helper's "state off" (no tailscaled)
		enum State { OFF, STARTING, NEEDS_LOGIN, NEEDS_APPROVAL, RUNNING, STOPPED } state = OFF;
		std::string backend;		// as said: tailscaled's word, or the helper's "off" / "starting"
		std::string error;			// the helper's error line
		std::string authUrl;		// the login link (NEEDS_LOGIN)
		std::string tailnet;		// its name, once logged in
		std::vector<std::string> health;	// tailscaled's warnings (the clock, the network, DERP)
		TsDevice self;
		std::vector<TsDevice> peers;	// in tailscaled's order
	};

	TsStatus parseStatus(const std::string& helperOutput);

	// the Tailscale row's value in Settings > Network. network: the handheld has an address (Wi-Fi, or a wired
	// adapter); without one, and with this device not connected to Tailscale's servers, it waits for Wi-Fi.
	// "state off" with no error is the moment before tailscaled starts (at boot, on switching it on): Starting.
	std::string rowWord(const TsStatus& s, bool network);

	// the device's name in the tailnet when Settings has none: the board's, from the device tree's model (as the helper
	// works it out at boot), banana when there is none
	std::string defaultName(const std::string& model);
	// a name typed on the keyboard: spaces at either end dropped, the others hyphens, capitals lowercase; "" when what
	// is left isn't a DNS label (a-z, 0-9 and hyphens, 1 to 63, no hyphen first or last)
	std::string normaliseName(const std::string& typed);

	// the login link as a QR code (Nayuki's qrcodegen: error correction M, or better where it fits the same size):
	// side x side modules, row after row, the 4-module quiet zone round it included, true for dark; empty when the
	// link can't be encoded
	std::vector<bool> loginQr(const std::string& url, int& side);

	// the devices page's list: this handheld first, then the devices online, then the others, each group by name
	std::vector<TsDevice> deviceList(const TsStatus& s);
	// when a device was last seen, from tailscaled's RFC 3339 time and now (seconds since 1970): "a moment ago",
	// "5 minutes ago", "an hour ago", "3 days ago", "on 2026-08-01"; "" when the time can't be read
	std::string seenAgo(const std::string& rfc3339, long long now);
	// tailscaled's OS names as people write them (linux: Linux); others as they are
	std::string osName(const std::string& os);
}
