//
// utility code to load HDR image files in Radiance's RGBE-encoded ".hdr" file format
//

#include <cstdio>
#include <cstring>

static const char *find_identifier(const char *line, const char *identifier)
{
    const size_t l = strlen(identifier);
    for (size_t i = 0; i < l; ++i)
        if (line[i] != identifier[i])
            return NULL;

    return &line[l];
}

/* header information */
typedef struct {
    unsigned int rx, ry; /* resolution */
    float gamma;
    float exposure;
    int xyz; /* color space is XYZ or RGB? */
    char comment[128];
} hdr_image_header_t;

/* error codes than can be reported */
typedef enum {
    HDR_SUCCESS = 0,
    HDR_ERROR_INVALID_HEADER = 1,
    HDR_ERROR_READING_HEADER = 2,
    HDR_ERROR_READING_PIXELS = 3,
    HDR_ERROR_ALLOCATION_FAILURE = 5,
    HDR_ERROR_WRITING_HEADER = 6,
    HDR_ERROR_WRITING_PIXELS = 7,
    HDR_ERROR_NUM_CODES = 9
} hdr_error_code_t;


static const char *s_rle_rgbe = "32-bit_rle_rgbe";
static const char *s_rle_xyze = "32-bit_rle_xyze";

static hdr_error_code_t hdr_read_image_header(
    hdr_image_header_t * const header, FILE * const fp)
{
    rewind(fp);
#define LINE_SIZE 2048
    char linebuf[LINE_SIZE];

    const char *line = fgets(linebuf, LINE_SIZE, fp);
    if (!line)
        return HDR_ERROR_INVALID_HEADER;
    
    if (line[0] != '#' && line[1] != '?')
        return HDR_ERROR_INVALID_HEADER;

    memset(header, 0, sizeof(hdr_image_header_t));
    header->gamma = 1.0f;
    header->exposure = 1.0f;

    while (1) {
        line = fgets(linebuf, LINE_SIZE, fp);

        if (line[0] == '#')
            continue;

        const char *c;
        if ((c = find_identifier(line, "EXPOSURE=")))
        {
            float f = 1.0f;
            if (sscanf(c, "%f", &f))
                header->exposure = f;
        }
        else if ((c = find_identifier(line, "GAMMA=")))
        {
            float f = 1.0f;
            if (sscanf(c, "%f", &f))
                header->gamma = f;
        }
        else if ((c = find_identifier(line, "FORMAT=")))
        {
                if (strncmp(c, s_rle_xyze, strlen(s_rle_xyze)) == 0)
                header->xyz = 1;
            else if (strncmp(c, s_rle_rgbe, strlen(s_rle_rgbe)) == 0)
                header->xyz = 0;
            else
                return HDR_ERROR_INVALID_HEADER;
        }           
        else if (line[0] == '-' || line[0] == '+')
        {
            //!! extract flips, order?
            const char *x = strchr(line, 'X');
            if (!x || (x <= line) || !sscanf(x + 1, "%u", &header->rx))
                return HDR_ERROR_INVALID_HEADER;

            const char *y = strchr(line, 'Y');
            if (!y || (y <= line) || !sscanf(y + 1, "%u", &header->ry))
                return HDR_ERROR_INVALID_HEADER;
            
            break;
        }
    }
    
    return HDR_SUCCESS;
}

/* read unencoded scanline */
static hdr_error_code_t hdr_read_image_scanline(
    unsigned char *const rgbe_buf,
    const unsigned int len,
    FILE *const fp)
{
    if (fread(rgbe_buf, 4, len, fp) != len)
        return HDR_ERROR_READING_PIXELS;
    
    return HDR_SUCCESS;
}

/* read run length encoded scanline */
static hdr_error_code_t hdr_read_image_scanline_rle(
    unsigned char *const rgbe_buf,
    const unsigned int len,
    FILE *const fp)
{
    int c = fgetc(fp);
    if (c == EOF)
        return HDR_ERROR_READING_PIXELS;

    if (c != 2) /* not really rle? */
    {
        ungetc(c, fp);
        return hdr_read_image_scanline(rgbe_buf, len, fp);
    }

    rgbe_buf[1] = fgetc(fp); /* green */
    rgbe_buf[2] = fgetc(fp); /* blue */
    c = fgetc(fp);

    if (rgbe_buf[1] != 2 || (rgbe_buf[2] & 128)) { /* not rle? */
        rgbe_buf[0] = 2;
        rgbe_buf[3] = c;
        return hdr_read_image_scanline(rgbe_buf + 4, len - 1, fp);
    }
    
    const unsigned int l = rgbe_buf[2] << 8 | c;
    if (l != len)
        return HDR_ERROR_READING_PIXELS;

    for (unsigned int i = 0; i < 4; ++i) /* components */
    {
        for (unsigned pos = 0; pos < len;) /* position in scanline */
        {
            int num = fgetc(fp);
            if (num == EOF)
                return HDR_ERROR_READING_PIXELS;
            
            if (num > 128) /* run start? */
            {
                num &= 127;
                const int val = fgetc(fp);
                if (val == EOF)
                    return HDR_ERROR_READING_PIXELS;
                if (pos + num > len)
                    return HDR_ERROR_READING_PIXELS;
                
                for (int j = 0; j < num; ++j)
                {
                    rgbe_buf[pos * 4 + i] = val;
                    ++pos;
                }
            }
            else /* non-run: read num chars */
            {
                if (pos + num > len)
                    return HDR_ERROR_READING_PIXELS;

                for (int j = 0; j < num; ++j)
                {
                    const int val = fgetc(fp);
                    if (val == EOF)
                        return HDR_ERROR_READING_PIXELS;

                    rgbe_buf[pos * 4 + i] = val;
                    ++pos;
                }
            }
        }
    }
    return HDR_SUCCESS;
}

/* convert RGBE to floating point color */
static void hdr_rgbe_to_color(
    float rgb[3],
    const unsigned char *const rgbe)
{
    if (rgbe[3] == 0)
        rgb[0] = rgb[1] = rgb[2] = 0.0f;
    else
    {  
        union {
            float f;
            unsigned int i;
        } u;

        u.i = (((int)rgbe[3] - 9) << 23) & 0x7f800000;

        rgb[0] = (float)(rgbe[0] + 0.5f) * u.f;
        rgb[1] = (float)(rgbe[1] + 0.5f) * u.f;
        rgb[2] = (float)(rgbe[2] + 0.5f) * u.f;
    }
}

static hdr_error_code_t hdr_read_image_pixels_float(
    const hdr_image_header_t * const header,
    float * const pixels,
    FILE * const fp,
    const bool rgba)
{
    /* small/large scanlines cannot be run-length encoded */
    const int rle = !(header->rx < 8 || header->rx > 0x7fff);
    const unsigned int ncomp = rgba ? 4 : 3;

    unsigned char *rgbe_buf = (unsigned char *)malloc(header->rx * 4);

    for (unsigned int j = 0; j < header->ry; ++j)
    {
        hdr_error_code_t code;
        if (rle)
            code = hdr_read_image_scanline_rle(rgbe_buf, header->rx, fp);
        else
            code = hdr_read_image_scanline(rgbe_buf, header->rx, fp);
        if (code != HDR_SUCCESS)
        {
            free(rgbe_buf);
            return code;
        }

        for (unsigned int i = 0; i < header->rx; ++i)
            hdr_rgbe_to_color(
                pixels + ncomp * (j * header->rx + i),
                rgbe_buf + 4 * i);
     }
    free(rgbe_buf);

    return HDR_SUCCESS;
}

static bool load_hdr_float4(float **pixels, unsigned int *rx, unsigned int *ry, const char *filename)
{
    *pixels = NULL;
    *rx = *ry = 0;
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return false;
    
    hdr_image_header_t header;
    if (hdr_read_image_header(&header, fp) != HDR_SUCCESS) {
        fclose(fp);
        return false;
    }
    
    *pixels = (float *)calloc(header.rx * header.ry, sizeof(float) * 4);

    if (hdr_read_image_pixels_float(&header, *pixels, fp, true) != HDR_SUCCESS) {
        fclose(fp);
        free(*pixels);
        return false;
    }

    fclose(fp);
    *rx = header.rx;
    *ry = header.ry;
    
    return true;
}

