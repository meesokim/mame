/*
	���g�p�Z�N�^���N���A����
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char fatbuf[256];

void usage()
{
	puts(
"2D-File�̖��g�p�N���X�^���N���A���܂�\n"
"usage: wipe2d <filename>[.2D]");
	exit(1);
}

void invalid(char *s)
{
	printf("wipe2d: %s\n", s); exit(1);
}


/* �t�@�C���G���g���̃S�~������ */
void wipe_dirent(char *mp, FILE *fp)
{
	int i, idx;

	fseek(fp, 0x25000L, SEEK_SET);
	fread(mp, 1, 0x800, fp);
	idx = 0;
	for(i=0;i<0x80;i++)
	{
		if (!mp[idx]) memset(mp+idx, 0xff, 0x10);
		idx += 0x10;
	}
	fseek(fp, -0x800L, SEEK_CUR);
	fwrite(mp, 1, 0x800, fp);

	fseek(fp, 0, SEEK_CUR);
	fread(mp, 1, 0x400, fp);
	idx = 0;
	for(i=0;i<0x40;i++)
	{
		if (!mp[idx]) memset(mp+idx, 0xff, 0x10);
		idx += 0x10;
	}
	fseek(fp, -0x400L, SEEK_CUR);
	fwrite(mp, 1, 0x400, fp);
}


void wipe_cluster(char *mp, FILE *fp)
{
	int i,c;

	/* FAT-table�ǂݍ��� */
	fseek(fp, 0x25D00L, SEEK_SET);
	fread(fatbuf, 1, 0x100, fp);

	fseek(fp, 0, SEEK_SET);
	for(i=0;i<0xa0;i++)
	{
		c = fatbuf[i];

		if (c == 0xfe || c < 0xa0 || c == 0xc8)
		{	/* �V�X�e���̈�or�m�[�}���̈�͉������Ȃ� */
			fseek(fp, 0x800L, SEEK_CUR);
			continue;
		}
		else if (c == 0xff || c == 0xec)
		{	/* ���g�p�N���X�^�Ȃ�S�� $ff�Ŗ��߂� */
			memset(mp, 0xff, 0x800);
			fwrite(mp, 1, 0x800, fp);
			continue;
		}
		else if (c >= 0xc1 && c <= 0xc7)
		{	/* �r���ŏI����Ă���Z�N�^�� $ff�Ŗ��߂� */
			int a;

			fseek(fp, 0, SEEK_CUR);
			fread(mp, 1, 0x800, fp);
			fseek(fp, -0x800L, SEEK_CUR);
			a = c - 0xc0;
			memset(mp+(a*0x100), 0xff, (8-a)*0x100);
			fwrite(mp, 1, 0x800, fp);
		}
		else
		{	/* �͈͊O�̃f�[�^�Ȃ�G���[ */
			printf("FAT-No:%d  DATA:%02X  �f�[�^���s���ł�\n", i, c);
			return;
		}
	}
}


int main(int argc, char **argv)
{
	char *mp;
	FILE *fp;

	if (argc == 1) usage();

	mp = malloc(0x800);
	if (!mp) invalid("malloc error");

	fp = fopen(argv[1], "r+b");
	if (!fp) invalid("fopen error");

	/* �t�@�C���G���g���̃N���A */
	wipe_dirent(mp, fp);

	/* �N���X�^�P�ʂŃS�~������ */
	wipe_cluster(mp, fp);

	free(mp);
	fclose(fp);
	return 0;
}
