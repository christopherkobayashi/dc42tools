#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

struct dc42_header {
	u_int8_t	image_name_length;
	char		image_name[63];
	u_int32_t	data_size;
	u_int32_t	tag_size;
	u_int32_t	data_checksum;
	u_int32_t	tag_checksum;
	u_int8_t	encoding;
	u_int8_t	format;
	u_int16_t	magic_number;
};

enum encoding {
	gcr_ssdd, /* 400k */
	gcr_dsdd, /* 800k */
	mfm_dsdd, /* 720k */
	mfm_dshd  /* 1440k */
};

int main(int argc, char **argv)
{
	struct dc42_header header;

	FILE *inputfile = NULL;
	FILE *outputfile = NULL;
	char outputfilename[255];
	struct stat inputfile_stat;
	u_int8_t *file_buffer = NULL;
	u_int32_t rv = 0;
	u_int16_t check;
	int i;

	if (argc != 2) {
		printf("usage: makedc42 <inputfile>\n");
		return -1;
	}

	memset(&header, 0x00, sizeof(header));

	if ( (inputfile = fopen(argv[1], "r")) == NULL) {
		printf("%s: does not exist.\n", argv[1]);
		return -1;
	}

	if ( (stat(argv[1], &inputfile_stat)) ) {
		printf("%s: could not stat\n", argv[1]);
	}

	printf("%s: size is %li\n", argv[1], inputfile_stat.st_size);

	file_buffer = calloc(1, inputfile_stat.st_size);
	if (!file_buffer) {
		printf("%s: could not allocate buffer\n", argv[1]);
		fclose(inputfile);
		return -1;
	}

	printf("%s: read %li bytes into buffer\n", argv[1],
	    fread(file_buffer, 1, inputfile_stat.st_size, inputfile));

	fclose(inputfile);

	/* consistency checks */
	check = (file_buffer[0x400] << 8) + file_buffer[0x401];
	if (check != 0x4244) {
		printf("%s: VIB failed consistency check (%x)\n", argv[1], check);
		free(file_buffer);
		return -1;
	}

	header.magic_number = htons(0x0100);
	header.image_name_length = strlen( (char *)&file_buffer[0x425]);
	strncpy( (char *)&header.image_name, (char *)&file_buffer[0x425], 27);

	printf("%s: embedded filename is \"%s\" (length %i)\n", argv[1], header.image_name, header.image_name_length);
	snprintf(outputfilename, 255, "%s-dc42.img", header.image_name);
	printf("%s: output filename is \"%s\"\n", argv[1], outputfilename);

	switch (inputfile_stat.st_size) {
		case 1474560:
		  header.encoding = mfm_dshd;
		  header.format = 0x22;
		  break;
		case 819200:
		  header.encoding = gcr_dsdd;
		  header.format = 0x22;
		  break;
		case 737280:
		  header.encoding = mfm_dsdd;
		  header.format = 0x22;
		  break;
		case 409600:
		  header.encoding = gcr_ssdd;
		  header.format = 0x02;
		  break;
		default:
		  printf("%s: unknown size, not doing format/encoding\n", argv[1]);
	}

	header.data_size = htonl(inputfile_stat.st_size);

	for (i = 0; i < inputfile_stat.st_size; i += 2) {
		rv += (file_buffer[i] << 8) + file_buffer[i+1];
    		rv = (rv >> 1) + (rv << 31);
	}

	printf("%s: calculated checksum %x\n", argv[1], rv);

	header.data_checksum = htonl(rv);

	outputfile = fopen(outputfilename, "w");

	if (!outputfile) {
		printf("%s: could not open output file\n", outputfilename);
		free(file_buffer);
		return -1;
	}

	fwrite(&header, sizeof(header), 1, outputfile);
	fwrite(file_buffer, inputfile_stat.st_size, 1, outputfile);

	if (file_buffer != NULL)
		free(file_buffer);

	if (outputfile != NULL)
		fclose(outputfile);	

	return 0;
}
