#include "crypto/initialization_vector.h"
#include "crypto/aes_gcm.h"

/* Using test cases in:
 * https://api.semanticscholar.org/CorpusID:6053538
 *
 * NOTE: The GCM standard describes support for a variable-length IV, that is
 * then expanded to a 128-bit initial value for counter mode called Y0. Because
 * this expansion is constly and unneeded it is not implemented in TDMH as we
 * compute the values for Y0 directly.  All the test values for the IVs are
 * referred to as Y0 in the test cases listed in the document.
 *
 * */

using namespace std;
using namespace mxnet;

void test4() {
    unsigned char key[16] = {0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c, 0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08};
    unsigned char iv_raw[16] = {0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad, 0xde, 0xca, 0xf8, 0x88, 0x00, 0x00, 0x00, 0x01};
    IV iv(iv_raw);
    unsigned char ptx[60] = {0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5, 0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a, 0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda, 0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72, 0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53, 0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25, 0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57, 0xba, 0x63, 0x7b, 0x39}; 
    unsigned char ctx[60] = {0};
    unsigned char slot[16] = {0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef};
    unsigned char auth[4] = {0xab, 0xad, 0xda, 0xd2};
    unsigned char ctx_result[60] = {0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24, 0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c, 0xe3, 0xaa, 0x21, 0x2f, 0x2c, 0x02, 0xa4, 0xe0, 0x35, 0xc1, 0x7e, 0x23, 0x29, 0xac, 0xa1, 0x2e, 0x21, 0xd5, 0x14, 0xb2, 0x54, 0x66, 0x93, 0x1c, 0x7d, 0x8f, 0x6a, 0x5a, 0xac, 0x84, 0xaa, 0x05, 0x1b, 0xa3, 0x0b, 0x39, 0x6a, 0x0a, 0xac, 0x97, 0x3d, 0x58, 0xe0, 0x91};

    unsigned char tag[16] = {0};
    unsigned char tag_result[16] = {0x5b, 0xc9, 0x4f, 0xbc, 0x32, 0x21, 0xa5, 0xdb, 0x94, 0xfa, 0xe9, 0x5a, 0xe7, 0x12, 0x1a, 0x47};
    AesGcm gcm(key, iv);
    gcm.setSlotInfo(slot);
    gcm.encryptAndComputeTag(tag, ctx, ptx, 60, auth, 4);

    bool success = true;
    for(int i=0; i<60; i++) {
        if(ctx[i] != ctx_result[i]) success = false;
    }
    if(success) printf("Encryption success!\n\n\n");
    else printf("Encryption FAIL :(\n\n\n");

    success = true;
    for(int i=0; i<16; i++) {
        if(tag[i] != tag_result[i]) success = false;
    }
    if(success) printf("GCM success!\n\n\n");
    else printf("GCM FAIL :(\n\n\n");

}

int main() {
    test4();
}
