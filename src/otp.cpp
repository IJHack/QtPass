#include "otp.h"

#include <iostream>
#include <stdexcept>
#include <cassert>

namespace OTP {
	namespace Bytes {
		void clearByteString(ByteString *bstr) {
			volatile Byte * bs = const_cast<volatile Byte *>(bstr->data());
			for (size_t i = 0; i < bstr->size(); ++i) {
				bs[i] = Byte(0);
			}
		}

		void swizzleByteStrings(ByteString * target, ByteString * source) {
			clearByteString(target);
			target->assign(*source);
			clearByteString(source);
		}

		static char nibbleToLCHex(uint8_t nib) {
			if (nib < 0xa) {
				return static_cast<char>(nib + '0');
			} else if (nib < 0x10) {
				return static_cast<char>((nib - 10) + 'a');
			} else {
				assert(0 && "not actually a nibble");
				return '\0';
			}
		}

		static uint8_t hexToNibble(char c) {
			if (c >= '0' && c <= '9') {
				return static_cast<uint8_t>(c - '0');
			} else if (c >= 'A' && c <= 'F') {
				return static_cast<uint8_t>(c - 'A' + 10);
			} else if (c >= 'a' && c <= 'f') {
				return static_cast<uint8_t>(c - 'a' + 10);
			} else {
				assert(0 && "not actually a hex digit");
				return 0xff;
			}
		}

		std::string toHexString(const ByteString & bstr) {
			std::string ret;
			for (Byte b : bstr) {
				ret.push_back(nibbleToLCHex((b >> 4) & 0x0F));
				ret.push_back(nibbleToLCHex((b >> 0) & 0x0F));
			}
			return ret;
		}

		ByteString fromHexStringSkipUnknown(const std::string & str) {
			std::string hstr;
			for (char c : str) {
				if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
					hstr.push_back(c);
				}
				// ignore otherwise
			}
			if (hstr.size() % 2 != 0) {
				throw std::invalid_argument("hex string (unknown characters ignored) length not divisible by 2");
			}
			ByteString ret;
			for (size_t i = 0; i < hstr.size(); i += 2) {
				uint8_t top = hexToNibble(hstr[i+0]);
				uint8_t btm = hexToNibble(hstr[i+1]);
				ret.push_back((top << 4) | btm);
			}
			return ret;
		}

		Bytes::ByteString u32beToByteString(uint32_t num) {
			Bytes::ByteString ret;
			ret.push_back((num >> 24) & 0xFF);
			ret.push_back((num >> 16) & 0xFF);
			ret.push_back((num >>  8) & 0xFF);
			ret.push_back((num >>  0) & 0xFF);
			return ret;
		}

		Bytes::ByteString u64beToByteString(uint64_t num) {
			Bytes::ByteString left  = u32beToByteString((num >> 32) & 0xFFFFFFFF);
			Bytes::ByteString right = u32beToByteString((num >>  0) & 0xFFFFFFFF);
			return left + right;
		}

		static ByteString b32ChunkToBytes(const std::string & str) {
			ByteString ret;
			uint64_t whole = 0x00;
			size_t padcount = 0;
			size_t finalcount;
		
			if (str.length() != 8) {
				throw std::invalid_argument("incorrect length of base32 chunk");
			}
			size_t i;
			for (i = 0; i < 8; ++i) {
				char c = str[i];
				uint64_t bits;
				if (c == '=') {
					bits = 0;
					++padcount;
				} else if (padcount > 0) {
					throw std::invalid_argument("padding character followed by non-padding character");
				} else if (c >= 'A' && c <= 'Z') {
					bits = static_cast<Byte>(c - 'A');
				} else if (c >= '2' && c <= '7') {
					bits = static_cast<Byte>(c - '2' + 26);
				} else {
					throw std::invalid_argument("not a base32 character: " + std::string(1, c));
				}
				// shift into the chunk
				whole |= (bits << ((7-i)*5));
			}
			switch (padcount) {
				case 0:
					finalcount = 5;
				break;
				case 1:
					finalcount = 4;
				break;
				case 3:
					finalcount = 3;
				break;
				case 4:
					finalcount = 2;
				break;
				case 6:
					finalcount = 1;
				break;
				default:
					throw std::invalid_argument("invalid number of padding characters");
			}
			for (i = 0; i < finalcount; ++i) {
				// shift out of the chunk
				ret.push_back(static_cast<Byte>((whole >> ((4-i)*8)) & 0xFF));
			}
			return ret;
		}

		static inline uint64_t u64(uint8_t n) {
			return static_cast<uint64_t>(n);
		}

		static std::string bytesToB32Chunk(const ByteString & bs) {
			if (bs.size() < 1 || bs.size() > 5) {
				throw std::invalid_argument("need a chunk of at least 1 and at most 5 bytes");
			}
			uint64_t whole = 0x00;
			size_t putchars = 2;
			std::string ret;

			// shift into the chunk
			whole |= (u64(bs[0]) << 32);
			if (bs.size() > 1) {
				whole |= (u64(bs[1]) << 24);
				putchars += 2;  // at least 4
			}
			if (bs.size() > 2) {
				whole |= (u64(bs[2]) << 16);
				++putchars;  // at least 5
			}
			if (bs.size() > 3) {
				whole |= (u64(bs[3]) <<  8);
				putchars += 2;  // at least 7
			}
			if (bs.size() > 4) {
				whole |= u64(bs[4]);
				++putchars;  // at least 8
			}
		
			size_t i;
			for (i = 0; i < putchars; ++i) {
				// shift out of the chunk
				Byte val = (whole >> ((7-i)*5)) & 0x1F;
				// map bits to base32
				if (val < 26) {
					ret.push_back(static_cast<char>(val + 'A'));
				} else {
					ret.push_back(static_cast<char>(val - 26 + '2'));
				}
			}
			// pad
			for (i = putchars; i < 8; ++i) {
				ret.push_back('=');
			}
			return ret;
		}

		ByteString fromBase32(const std::string & b32str) {
			if (b32str.size() % 8 != 0) {
				throw std::invalid_argument("base32 string length not divisible by 8");
			}
			ByteString ret;
			for (size_t i = 0; i < b32str.size(); i += 8) {
				std::string sub(b32str, i, 8);
				ByteString chk = b32ChunkToBytes(sub);
				ret.append(chk);
			}
			return ret;
		}

		ByteString fromUnpaddedBase32(const std::string & b32str) {
			std::string newstr = b32str;
			while (newstr.size() % 8 != 0) {
				newstr.push_back('=');
			}
			return fromBase32(newstr);
		}

		std::string toBase32(const ByteString & bs) {
			std::string ret;
			size_t i, j, len;
			for (j = 0; j < bs.size() / 5; ++j) {
				i = j * 5;
				ByteString sub(bs, i, 5);
				std::string chk = bytesToB32Chunk(sub);
				ret.append(chk);
			}
			i = j * 5;
			len = bs.size() - i;
			if (len > 0) {
				// block of size < 5 remains
				ByteString sub(bs, i, std::string::npos);
				std::string chk = bytesToB32Chunk(sub);
				ret.append(chk);
			}
			return ret;
		}
	}

	static inline uint32_t lrot32(uint32_t num, uint8_t rotcount) {
		return (num << rotcount) | (num >> (32 - rotcount));
	}

	Bytes::ByteString sha1(const Bytes::ByteString & msg) {
		const size_t size_bytes = msg.size();
		const uint64_t size_bits = size_bytes * 8;
		Bytes::ByteString bstr = msg;
		Bytes::ByteStringDestructor asplode(&bstr);

		// the size of msg in bits is always even. adding the '1' bit will make
		// it odd and therefore incongruent to 448 modulo 512, so we can get
		// away with tacking on 0x80 and then the 0x00s.
		bstr.push_back(0x80);
		while (bstr.size() % (512/8) != (448/8)) {
			bstr.push_back(0x00);
		}
		// append the size in bits (uint64be)
		bstr.append(Bytes::u64beToByteString(size_bits));
		assert(bstr.size() % (512/8) == 0);
		// initialize the hash counters
		uint32_t h0 = 0x67452301;
		uint32_t h1 = 0xEFCDAB89;
		uint32_t h2 = 0x98BADCFE;
		uint32_t h3 = 0x10325476;
		uint32_t h4 = 0xC3D2E1F0;
		// for each 64-byte chunk
		for (size_t i = 0; i < bstr.size()/64; ++i) {
			Bytes::ByteString chunk(bstr.begin() + i*64, bstr.begin() + (i+1)*64);
			Bytes::ByteStringDestructor xplode(&chunk);
			uint32_t words[80];
			size_t j;
			// 0-15: the chunk as a sequence of 32-bit big-endian integers
			for (j = 0; j < 16; ++j) {
				words[j] = (chunk[4*j + 0] << 24) | (chunk[4*j + 1] << 16) | (chunk[4*j + 2] <<  8) | (chunk[4*j + 3] <<  0);
			}
			// 16-79: derivatives of 0-15
			for (j = 16; j < 32; ++j) {
				// unoptimized
				words[j] = lrot32(words[j-3] ^ words[j-8] ^ words[j-14] ^ words[j-16], 1);
			}
			for (j = 32; j < 80; ++j) {
				// Max Locktyuchin's optimization (SIMD)
				words[j] = lrot32(words[j-6] ^ words[j-16] ^ words[j-28] ^ words[j-32], 2);
			}
			// initialize hash values for the round
			uint32_t a = h0;
			uint32_t b = h1;
			uint32_t c = h2;
			uint32_t d = h3;
			uint32_t e = h4;
			// the loop
			for (j = 0; j < 80; ++j) {
				uint32_t f = 0, k = 0;
				if (j < 20) {
					f = (b & c) | ((~ b) & d);
					k = 0x5A827999;
				} else if (j < 40) {
					f = b ^ c ^ d;
					k = 0x6ED9EBA1;
				} else if (j < 60) {
					f = (b & c) | (b & d) | (c & d);
					k = 0x8F1BBCDC;
				} else if (j < 80) {
					f = b ^ c ^ d;
					k = 0xCA62C1D6;
				} else {
					assert(0 && "how did I get here?");
				}
				uint32_t tmp = lrot32(a, 5) + f + e + k + words[j];
				e = d;
				d = c;
				c = lrot32(b, 30);
				b = a;
				a = tmp;
			}
			// add that to the result so far
			h0 += a;
			h1 += b;
			h2 += c;
			h3 += d;
			h4 += e;
		}
		// assemble the digest
		Bytes::ByteString first  = Bytes::u32beToByteString(h0);
		Bytes::ByteStringDestructor x1(&first);
		Bytes::ByteString second = Bytes::u32beToByteString(h1);
		Bytes::ByteStringDestructor x2(&second);
		Bytes::ByteString third  = Bytes::u32beToByteString(h2);
		Bytes::ByteStringDestructor x3(&third);
		Bytes::ByteString fourth = Bytes::u32beToByteString(h3);
		Bytes::ByteStringDestructor x4(&fourth);
		Bytes::ByteString fifth  = Bytes::u32beToByteString(h4);
		Bytes::ByteStringDestructor x5(&fifth);
		return first + second + third + fourth + fifth;
	}

	Bytes::ByteString hmacSha1(const Bytes::ByteString & key, const Bytes::ByteString & msg, size_t blockSize = 64);

	Bytes::ByteString hmacSha1(const Bytes::ByteString & key, const Bytes::ByteString & msg, size_t blockSize) {
		Bytes::ByteString realKey = key;
		Bytes::ByteStringDestructor asplode(&realKey);
		if (realKey.size() > blockSize) {
			// resize by calculating hash
			Bytes::ByteString newRealKey = sha1(realKey);
			Bytes::swizzleByteStrings(&realKey, &newRealKey);
		}
		if (realKey.size() < blockSize) {
			// pad with zeroes
			realKey.resize(blockSize, 0x00);
		}
		// prepare the pad keys
		Bytes::ByteString innerPadKey = realKey;
		Bytes::ByteStringDestructor xplodeI(&innerPadKey);
		Bytes::ByteString outerPadKey = realKey;
		Bytes::ByteStringDestructor xplodeO(&outerPadKey);
		// transform the pad keys
		for (size_t i = 0; i < realKey.size(); ++i) {
			innerPadKey[i] = innerPadKey[i] ^ 0x36;
			outerPadKey[i] = outerPadKey[i] ^ 0x5c;
		}
		// sha1(outerPadKey + sha1(innerPadKey + msg))
		Bytes::ByteString innerMsg  = innerPadKey + msg;
		Bytes::ByteStringDestructor xplodeIM(&innerMsg);
		Bytes::ByteString innerHash = sha1(innerMsg);
		Bytes::ByteStringDestructor xplodeIH(&innerHash);
		Bytes::ByteString outerMsg  = outerPadKey + innerHash;
		Bytes::ByteStringDestructor xplodeOM(&outerMsg);
		return sha1(outerMsg);
	}

	Bytes::ByteString hmacSha1_64(const Bytes::ByteString & key, const Bytes::ByteString & msg) {
		return hmacSha1(key, msg, 64);
	}

	uint32_t hotp(const Bytes::ByteString & key, uint64_t counter, size_t digitCount, HmacFunc hmacf) {
		Bytes::ByteString msg = Bytes::u64beToByteString(counter);
		Bytes::ByteStringDestructor dmsg(&msg);
		Bytes::ByteString hmac = hmacf(key, msg);
		Bytes::ByteStringDestructor dhmac(&hmac);
		uint32_t digits10 = 1;
		for (size_t i = 0; i < digitCount; ++i) {
			digits10 *= 10;
		}
		// fetch the offset (from the last nibble)
		uint8_t offset = hmac[hmac.size()-1] & 0x0F;
		// fetch the four bytes from the offset
		Bytes::ByteString fourWord = hmac.substr(offset, 4);
		Bytes::ByteStringDestructor dfourWord(&fourWord);
		// turn them into a 32-bit integer
		uint32_t ret = (fourWord[0] << 24) | (fourWord[1] << 16) | (fourWord[2] <<  8) | (fourWord[3] <<  0);
		// snip off the MSB (to alleviate signed/unsigned troubles)
		// and calculate modulo digit count
		return (ret & 0x7fffffff) % digits10;
	}

	uint32_t totp(const Bytes::ByteString & key, uint64_t timeNow, uint64_t timeStart, uint64_t timeStep, size_t digitCount, HmacFunc hmacf) {
		uint64_t timeValue = (timeNow - timeStart) / timeStep;
		return hotp(key, timeValue, digitCount, hmacf);
	}
}

#if TEST_SHA1
int main(void) {
	using namespace OTP;
	const uint8_t * strEmpty = reinterpret_cast<const uint8_t *>("");
	const uint8_t * strDog   = reinterpret_cast<const uint8_t *>("The quick brown fox jumps over the lazy dog");
	const uint8_t * strCog   = reinterpret_cast<const uint8_t *>("The quick brown fox jumps over the lazy cog");
	const uint8_t * strKey   = reinterpret_cast<const uint8_t *>("key");

	Bytes::ByteString shaEmpty = sha1(Bytes::ByteString(strEmpty));
	Bytes::ByteString shaDog   = sha1(Bytes::ByteString(strDog));
	Bytes::ByteString shaCog   = sha1(Bytes::ByteString(strCog));

	Bytes::ByteString hmacShaEmpty  = hmacSha1(Bytes::ByteString(), Bytes::ByteString());
	Bytes::ByteString hmacShaKeyDog = hmacSha1(strKey, strDog);

	std::cout
		<< (Bytes::toHexString(shaEmpty) == "da39a3ee5e6b4b0d3255bfef95601890afd80709") << std::endl
		<< (Bytes::toHexString(shaDog)   == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12") << std::endl
		<< (Bytes::toHexString(shaCog)   == "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3") << std::endl
		<< std::endl
		<< (Bytes::toHexString(hmacShaEmpty)  == "fbdb1d1b18aa6c08324b7d64b71fb76370690e1d") << std::endl
		<< (Bytes::toHexString(hmacShaKeyDog) == "de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9") << std::endl
	<< std::endl;

	return 0;
}
#endif

#if TEST_OTP
int main(void) {
	using namespace OTP;

	uint64_t start  =  0;
	uint64_t step   = 30;
	uint8_t digitsH =  6;
	uint8_t digitsT =  8;
	const Bytes::ByteString key = reinterpret_cast<const uint8_t *>("12345678901234567890");

	std::cout
		<< (hotp(key, 0, digitsH) == 755224)
		<< (hotp(key, 1, digitsH) == 287082)
		<< (hotp(key, 2, digitsH) == 359152)
		<< (hotp(key, 3, digitsH) == 969429)
		<< (hotp(key, 4, digitsH) == 338314)
		<< (hotp(key, 5, digitsH) == 254676)
		<< (hotp(key, 6, digitsH) == 287922)
		<< (hotp(key, 7, digitsH) == 162583)
		<< (hotp(key, 8, digitsH) == 399871)
		<< (hotp(key, 9, digitsH) == 520489)
		<< (totp(key, 59, start, step, digitsT) == 94287082)
		<< (totp(key, 1111111109, start, step, digitsT) == 7081804)
		<< (totp(key, 1111111111, start, step, digitsT) == 14050471)
		<< (totp(key, 1234567890, start, step, digitsT) == 89005924)
		<< (totp(key, 2000000000, start, step, digitsT) == 69279037)
		<< (totp(key, 20000000000, start, step, digitsT) == 65353130)
	<< std::endl;

	const Bytes::ByteString tutestkey = reinterpret_cast<const uint8_t *>("HelloWorld");
	std::cout << totp(tutestkey, time(NULL), 0, 30, 6) << std::endl;

	return 0;
}
#endif