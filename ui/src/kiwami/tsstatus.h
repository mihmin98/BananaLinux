#pragma once
// banana-ui (kiwami): Tailscale's state, from the helper's `status` (kiwami/rootfs/usr/libexec/kiwami/tailscale):
// "state off" or "state starting" while there is no tailscaled to ask, "error <text>" for the last failure the helper
// knows, then `tailscale status --json` (its fields as tailscaled 1.98.8 writes them). Pure functions, no I/O.
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
}
