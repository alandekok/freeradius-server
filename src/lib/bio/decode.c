typedef struct {
	TALLOC_CTX	*ctx;
	fr_pair_list_t	*list;

	void			*decode_ctx;
	fr_tp_proto_decode_t	decode;
} fr_bio_decode_ctx_t;

typedef struct {
	TALLOC_CTX	*ctx;
	fr_pair_list_t	*list;

	void			*encode_ctx;
	fr_tp_proto_encode_t	encode;
} fr_bio_encode_ctx_t;

typedef struct {
	FR_BIO_COMMON;

	// talloc types for encode/decode ctx?
} fr_bio_proto_t;

