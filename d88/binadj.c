/*
	�A�h���X���t���̃o�C�i���t�@�C�����v���[���ȃo�C�i���ɕϊ�����B
	�A�h���X���� 0000.log �ɃZ�[�u�����
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

void invalid(char *s)
{
	fprintf(stderr, "binadj: %s\n", s); exit(1);
}


void binadj(char *fname)
{
	FILE *fp;
	unsigned c1,c2,fs;
	unsigned char *mp, *p;

	fp = fopen(fname, "rb");
	if (!fp) invalid("cannot open file");

	/* �t�@�C���T�C�Y��malloc���Ă����ɓǂ� */
	fs = (unsigned)filelength(fileno(fp));
	mp = malloc(fs);
	if (!mp) invalid("cannot malloc");
	fread(mp, 1, fs, fp);
	fclose(fp);

	/* ���O�t�@�C���������� */
	fp = fopen("0000.log", "at");
	if (!fp) invalid("cannot open logfile");
	c1 = mp[0] + mp[1]*256;
	c2 = mp[2] + mp[3]*256;
	fprintf(fp, "%04x - %04x :%s\n", c1, c2 - 1, fname);
	printf(     "%04x - %04x :%s\n", c1, c2 - 1, fname);
	fclose(fp);

	/* 4byte���炵�ď㏑�� */
	fp = fopen(fname, "wb");
	if (!fp) invalid("cannot create file");
	fwrite(mp+4, 1, c2 - c1, fp);
	fclose(fp);

	free(mp);
}


int main(int argc, char **argv)
{
	int i;

	if (argc == 1) { puts("usage: binadj <filename> [files...]"); return 0; }

	for(i=1;argv[i];i++) binadj(argv[i]);

	return 0;
}
