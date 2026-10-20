#pragma once
// banana-ui (kiwami): a small JSON reader, for `tailscale status --json`. The whole text into a tree of values:
// objects, arrays, strings (\u escapes to UTF-8), numbers, booleans and null. Malformed input is refused, never
// crashed on (nesting stops at 64 levels). A missing key or index reads as null, so lookups chain:
//   v["Self"]["TailscaleIPs"][0].str()
#include <string>
#include <utility>
#include <vector>

namespace banana::kiwami::json
{
	struct Value
	{
		enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
		bool b = false;
		double n = 0;
		std::string s;
		std::vector<Value> arr;
		std::vector<std::pair<std::string, Value>> obj;	// in the text's order

		const Value& operator[](const std::string& key) const;	// an object's member (the last one of that name)
		const Value& operator[](size_t i) const;				// an array's element
		size_t size() const { return type == ARR ? arr.size() : type == OBJ ? obj.size() : 0; }
		bool isNull() const { return type == NUL; }
		const std::string& str() const;						// "" for anything but a string
		bool boolean() const { return type == BOOL && b; }		// false for anything but true
	};

	// the text into v; false (v null) when it isn't one JSON value, with nothing after it but spaces
	bool parse(const std::string& text, Value& v);
}
