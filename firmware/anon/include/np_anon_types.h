/*
 * NeurOne Research Anonymization — Status Types
 * Document: NP-FW-EMMC-002 Rev A §D
 *
 * Status enum for the scratch-partition AES-256-CTR encryption layer.
 * Convention (project-wide): NP_ANON_OK = 0, all error codes negative.
 */

#ifndef NP_ANON_TYPES_H
#define NP_ANON_TYPES_H

typedef enum {
    NP_ANON_OK            =  0,  /* Success                                    */
    NP_ANON_ERR_TRNG      = -1,  /* TRNG failed to produce key or nonce        */
    NP_ANON_ERR_ENCRYPT   = -2,  /* AES-CTR crypt operation failed             */
    NP_ANON_ERR_EMMC      = -3,  /* eMMC scratch read/write/sanitize failed    */
    NP_ANON_ERR_PIPELINE  = -4,  /* Pipeline state error (e.g. not initialized)*/
    NP_ANON_ERR_INVALID   = -5   /* Invalid argument (null/length/range)       */
} np_anon_status_t;

#endif /* NP_ANON_TYPES_H */
