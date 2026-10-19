// banana-ui (kiwami): the JSON reader (json.h). Recursive descent over the text, one value at a time.
#include "json.h"
#include <cstdlib>
#include <cstring>

namespace banana::kiwami::json
{
	static const Value NONE;
	static const std::string EMPTY;

	const Value& Value::operator[](const std::string& key) const
	{
		if (type != OBJ) return NONE;
		for (size_t i = obj.size(); i-- > 0;)
			if (obj[i].first == key) return obj[i].second;
		return NONE;
	}

	const Value& Value::operator[](size_t i) const
	{
		return type == ARR && i < arr.size() ? arr[i] : NONE;
	}

	const std::string& Value::str() const
	{
		return type == STR ? s : EMPTY;
	}

	namespace
	{
		struct Reader
		{
			const char* p;
			const char* end;
			int depth = 0;

			void space() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
			bool word(const char* w)
			{
				size_t l = strlen(w);
				if ((size_t)(end - p) < l || memcmp(p, w, l) != 0) return false;
				p += l;
				return true;
			}
			static void utf8(std::string& o, unsigned c)
			{
				if (c < 0x80) o += (char)c;
				else if (c < 0x800) { o += (char)(0xc0 | (c >> 6)); o += (char)(0x80 | (c & 0x3f)); }
				else if (c < 0x10000) { o += (char)(0xe0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3f)); o += (char)(0x80 | (c & 0x3f)); }
				else
				{
					o += (char)(0xf0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3f));
					o += (char)(0x80 | ((c >> 6) & 0x3f)); o += (char)(0x80 | (c & 0x3f));
				}
			}
			bool hex4(unsigned& c)
			{
				if (end - p < 4) return false;
				c = 0;
				for (int i = 0; i < 4; i++, p++)
				{
					char h = *p;
					c <<= 4;
					if (h >= '0' && h <= '9') c |= h - '0';
					else if (h >= 'a' && h <= 'f') c |= h - 'a' + 10;
					else if (h >= 'A' && h <= 'F') c |= h - 'A' + 10;
					else return false;
				}
				return true;
			}
			bool string(std::string& o)
			{
				if (p >= end || *p != '"') return false;
				p++;
				while (p < end && *p != '"')
				{
					unsigned char ch = (unsigned char)*p++;
					if (ch < 0x20) return false;	// a control character must be escaped
					if (ch != '\\') { o += (char)ch; continue; }
					if (p >= end) return false;
					switch (*p++)
					{
					case '"': o += '"'; break;
					case '\\': o += '\\'; break;
					case '/': o += '/'; break;
					case 'b': o += '\b'; break;
					case 'f': o += '\f'; break;
					case 'n': o += '\n'; break;
					case 'r': o += '\r'; break;
					case 't': o += '\t'; break;
					case 'u':
					{
						unsigned c;
						if (!hex4(c)) return false;
						// a surrogate pair is one character; a half on its own becomes U+FFFD
						if (c >= 0xd800 && c < 0xdc00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u')
						{
							const char* back = p;
							unsigned lo;
							p += 2;
							if (hex4(lo) && lo >= 0xdc00 && lo < 0xe000) c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00);
							else { p = back; c = 0xfffd; }
						}
						else if (c >= 0xd800 && c < 0xe000) c = 0xfffd;
						utf8(o, c);
						break;
					}
					default: return false;
					}
				}
				if (p >= end) return false;
				p++;
				return true;
			}
			bool number(double& n)
			{
				// JSON's grammar checked here; strtod then reads the same characters
				const char* s = p;
				if (p < end && *p == '-') p++;
				if (p >= end || *p < '0' || *p > '9') return false;
				if (*p == '0') p++; else while (p < end && *p >= '0' && *p <= '9') p++;
				if (p < end && *p == '.')
				{
					p++;
					if (p >= end || *p < '0' || *p > '9') return false;
					while (p < end && *p >= '0' && *p <= '9') p++;
				}
				if (p < end && (*p == 'e' || *p == 'E'))
				{
					p++;
					if (p < end && (*p == '+' || *p == '-')) p++;
					if (p >= end || *p < '0' || *p > '9') return false;
					while (p < end && *p >= '0' && *p <= '9') p++;
				}
				n = strtod(std::string(s, p).c_str(), nullptr);
				return true;
			}
			bool value(Value& v)
			{
				space();
				if (p >= end) return false;
				switch (*p)
				{
				case '{':
				{
					if (++depth > 64) return false;
					p++;
					v.type = Value::OBJ;
					space();
					if (p < end && *p == '}') { p++; depth--; return true; }
					for (;;)
					{
						space();
						std::pair<std::string, Value> m;
						if (!string(m.first)) return false;
						space();
						if (p >= end || *p++ != ':') return false;
						if (!value(m.second)) return false;
						v.obj.push_back(std::move(m));
						space();
						if (p >= end) return false;
						if (*p == ',') { p++; continue; }
						if (*p++ != '}') return false;
						depth--;
						return true;
					}
				}
				case '[':
				{
					if (++depth > 64) return false;
					p++;
					v.type = Value::ARR;
					space();
					if (p < end && *p == ']') { p++; depth--; return true; }
					for (;;)
					{
						v.arr.emplace_back();
						if (!value(v.arr.back())) return false;
						space();
						if (p >= end) return false;
						if (*p == ',') { p++; continue; }
						if (*p++ != ']') return false;
						depth--;
						return true;
					}
				}
				case '"': v.type = Value::STR; return string(v.s);
				case 't': v.type = Value::BOOL; v.b = true; return word("true");
				case 'f': v.type = Value::BOOL; return word("false");
				case 'n': return word("null");
				default: v.type = Value::NUM; return number(v.n);
				}
			}
		};
	}

	bool parse(const std::string& text, Value& v)
	{
		Reader r{ text.data(), text.data() + text.size() };
		v = Value();
		bool ok = r.value(v);
		r.space();
		if (!ok || r.p != r.end) { v = Value(); return false; }
		return true;
	}
}
