#ifndef sha3_h
#define sha3_h

/**
  *  Function to compute SHAKE128 on the input message with any output length.
  */
void FIPS202_SHAKE128(const unsigned char *input, unsigned int inputByteLen, unsigned char *output, int outputByteLen);

/**
  *  Function to compute SHAKE256 on the input message with any output length.
  */
void FIPS202_SHAKE256(const unsigned char *input, unsigned int inputByteLen, unsigned char *output, int outputByteLen);

/**
  *  Function to compute SHA3-224 on the input message. The output length is fixed to 28 bytes.
  */
void FIPS202_SHA3_224(const unsigned char *input, unsigned int inputByteLen, unsigned char *output);

/**
  *  Function to compute SHA3-256 on the input message. The output length is fixed to 32 bytes.
  */
void FIPS202_SHA3_256(const unsigned char *input, unsigned int inputByteLen, unsigned char *output);

/**
  *  Function to compute SHA3-384 on the input message. The output length is fixed to 48 bytes.
  */
void FIPS202_SHA3_384(const unsigned char *input, unsigned int inputByteLen, unsigned char *output);

/**
  *  Function to compute SHA3-512 on the input message. The output length is fixed to 64 bytes.
  */
void FIPS202_SHA3_512(const unsigned char *input, unsigned int inputByteLen, unsigned char *output);

#endif