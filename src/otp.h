#ifndef OTP_H
#define OTP_H

#include <string>

namespace OTP {
	namespace Bytes {
		typedef uint8_t Byte;
		typedef std::basic_string<Byte> ByteString;

		void clearByteString(ByteString *bstr);
		void swizzleByteString(ByteString *target, ByteString *source);
		std::string toHexString(const ByteString &bstr);
		ByteString u32beToByteString(uint32_t num);
		ByteString u64beToByteString(uint64_t num);
		ByteString fromBase32(const std::string &b32str);
		ByteString fromUnpaddedBase32(const std::string &b32str);
		std::string toBase32(const ByteString &b32str);

		class ByteStringDestructor {
			private:
				/** The byte string to clear. */
				ByteString * m_bs;

			public:
				ByteStringDestructor(ByteString * bs) : m_bs(bs) {}
				~ByteStringDestructor() { clearByteString(m_bs); }
		};
	}
	typedef Bytes::ByteString (*HmacFunc)(const Bytes::ByteString &, const Bytes::ByteString &);

	Bytes::ByteString sha1(const Bytes::ByteString &msg);
	Bytes::ByteString hmacSha1(const Bytes::ByteString &key, const Bytes::ByteString &msg, size_t blockSize = 64);

	Bytes::ByteString hmacSha1_64(const Bytes::ByteString &key, const Bytes::ByteString &msg);
	uint32_t hotp(const Bytes::ByteString &key, uint64_t counter, size_t digitCount = 6, HmacFunc hmac = hmacSha1_64);
	uint32_t totp(const Bytes::ByteString & key, uint64_t timeNow, uint64_t timeStart, uint64_t timeStep, size_t digitCount = 6, HmacFunc hmac = hmacSha1_64);
}

#endif

