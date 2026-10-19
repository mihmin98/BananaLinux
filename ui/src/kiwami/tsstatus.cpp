// banana-ui (kiwami): Tailscale's state from the helper's status output (tsstatus.h).
#include "tsstatus.h"
#include "json.h"
#include "qrcodegen.hpp"

namespace banana::kiwami
{
	static TsDevice device(const json::Value& v)
	{
		TsDevice d;
		d.hostName = v["HostName"].str();
		d.dnsName = v["DNSName"].str();
		d.os = v["OS"].str();
		std::string label = d.dnsName.substr(0, d.dnsName.find('.'));
		d.name = !label.empty() ? label : d.hostName;
		const json::Value& ips = v["TailscaleIPs"];
		for (size_t i = 0; i < ips.size(); i++)
		{
			const std::string& ip = ips[i].str();
			std::string& slot = ip.find(':') == std::string::npos ? d.ipv4 : d.ipv6;
			if (slot.empty()) slot = ip;
		}
		d.online = v["Online"].boolean();
		d.expired = v["Expired"].boolean();
		d.lastSeen = v["LastSeen"].str();
		if (d.lastSeen.compare(0, 4, "0001") == 0) d.lastSeen.clear();	// Go's zero time: never
		return d;
	}

	TsStatus parseStatus(const std::string& out)
	{
		TsStatus s;
		// the helper's own lines come first, then the JSON
		size_t at = 0;
		while (at < out.size() && out[at] != '{')
		{
			size_t nl = out.find('\n', at);
			std::string ln = out.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
			if (!ln.empty() && ln.back() == '\r') ln.pop_back();
			if (ln.compare(0, 6, "state ") == 0) s.backend = ln.substr(6);
			else if (ln.compare(0, 6, "error ") == 0) s.error = ln.substr(6);
			if (nl == std::string::npos) { at = out.size(); break; }
			at = nl + 1;
		}
		json::Value v;
		if (at < out.size() && json::parse(out.substr(at), v))
		{
			s.backend = v["BackendState"].str();
			s.authUrl = v["AuthURL"].str();
			s.tailnet = v["CurrentTailnet"]["Name"].str();
			const json::Value& h = v["Health"];
			for (size_t i = 0; i < h.size(); i++) if (!h[i].str().empty()) s.health.push_back(h[i].str());
			if (v["Self"].type == json::Value::OBJ) s.self = device(v["Self"]);
			const json::Value& peers = v["Peer"];
			for (auto& p : peers.obj) s.peers.push_back(device(p.second));
		}
		const std::string& b = s.backend;
		s.state = b == "off" ? TsStatus::OFF : b == "NeedsLogin" ? TsStatus::NEEDS_LOGIN : b == "NeedsMachineAuth" ? TsStatus::NEEDS_APPROVAL
			: b == "Running" ? TsStatus::RUNNING : b == "Stopped" ? TsStatus::STOPPED : TsStatus::STARTING;	// (starting, NoState, Starting, unreadable)
		return s;
	}

	std::string rowWord(const TsStatus& s, bool network)
	{
		bool offline = !network && !s.self.online;
		switch (s.state)
		{
		case TsStatus::OFF: return s.error.empty() ? "Starting" : "Error";
		case TsStatus::NEEDS_APPROVAL: return "Waiting for approval";
		case TsStatus::STOPPED: return "Stopped";
		case TsStatus::NEEDS_LOGIN: return offline ? "Waiting for Wi-Fi" : s.self.expired ? "Log in again" : "Log in";
		case TsStatus::RUNNING: return offline ? "Waiting for Wi-Fi" : !s.self.ipv4.empty() ? s.self.ipv4 : "Connected";
		case TsStatus::STARTING: break;
		}
		return offline ? "Waiting for Wi-Fi" : "Starting";
	}

	std::string defaultName(const std::string& model)
	{
		if (model.find("RG DS Plus") != std::string::npos) return "banana-rgds-plus";
		if (model.find("RG DS") != std::string::npos) return "banana-rgds";
		return "banana";
	}

	std::string normaliseName(const std::string& typed)
	{
		size_t a = typed.find_first_not_of(' '), b = typed.find_last_not_of(' ');
		if (a == std::string::npos) return "";
		std::string n;
		for (size_t i = a; i <= b; i++)
		{
			char c = typed[i];
			if (c == ' ') c = '-';
			else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
			else if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '-') return "";
			n += c;
		}
		if (n.size() > 63 || n.front() == '-' || n.back() == '-') return "";
		return n;
	}

	std::vector<bool> loginQr(const std::string& url, int& side)
	{
		side = 0;
		if (url.empty()) return {};
		try
		{
			qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(url.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
			int n = qr.getSize();
			side = n + 8;
			std::vector<bool> m((size_t)side * side, false);
			for (int y = 0; y < n; y++)
				for (int x = 0; x < n; x++) m[(size_t)(y + 4) * side + x + 4] = qr.getModule(x, y);
			return m;
		}
		catch (const std::exception&) { side = 0; return {}; }	// (longer than a QR code holds)
	}
}
