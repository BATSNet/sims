# libwebp v1.5.0 - encoder-only port for ESP-IDF
# Generic C implementation only (no SSE/NEON/MIPS)

set(WEBP_SRCS
    # Encoder
    "src/enc/alpha_enc.c"
    "src/enc/analysis_enc.c"
    "src/enc/backward_references_cost_enc.c"
    "src/enc/backward_references_enc.c"
    "src/enc/config_enc.c"
    "src/enc/cost_enc.c"
    "src/enc/filter_enc.c"
    "src/enc/frame_enc.c"
    "src/enc/histogram_enc.c"
    "src/enc/iterator_enc.c"
    "src/enc/near_lossless_enc.c"
    "src/enc/picture_csp_enc.c"
    "src/enc/picture_enc.c"
    "src/enc/picture_psnr_enc.c"
    "src/enc/picture_rescale_enc.c"
    "src/enc/picture_tools_enc.c"
    "src/enc/predictor_enc.c"
    "src/enc/quant_enc.c"
    "src/enc/syntax_enc.c"
    "src/enc/token_enc.c"
    "src/enc/tree_enc.c"
    "src/enc/vp8l_enc.c"
    "src/enc/webp_enc.c"

    # DSP (generic C only)
    "src/dsp/alpha_processing.c"
    "src/dsp/cost.c"
    "src/dsp/cpu.c"
    "src/dsp/dec.c"
    "src/dsp/dec_clip_tables.c"
    "src/dsp/enc.c"
    "src/dsp/filters.c"
    "src/dsp/lossless.c"
    "src/dsp/lossless_enc.c"
    "src/dsp/rescaler.c"
    "src/dsp/ssim.c"
    "src/dsp/upsampling.c"
    "src/dsp/yuv.c"

    # Utils
    "src/utils/bit_reader_utils.c"
    "src/utils/bit_writer_utils.c"
    "src/utils/color_cache_utils.c"
    "src/utils/filters_utils.c"
    "src/utils/huffman_encode_utils.c"
    "src/utils/huffman_utils.c"
    "src/utils/palette.c"
    "src/utils/quant_levels_utils.c"
    "src/utils/random_utils.c"
    "src/utils/rescaler_utils.c"
    "src/utils/thread_utils.c"
    "src/utils/utils.c"

    # SharpYUV (generic C only)
    "sharpyuv/sharpyuv.c"
    "sharpyuv/sharpyuv_cpu.c"
    "sharpyuv/sharpyuv_csp.c"
    "sharpyuv/sharpyuv_dsp.c"
    "sharpyuv/sharpyuv_gamma.c"
)

idf_component_register(
    SRCS ${WEBP_SRCS}
    INCLUDE_DIRS "src" "."
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WEBP_USE_THREAD=0
)
