
#include "dvr.h"

void str_trimtail(char *line)
{
	int len;
	len = strlen(line);
        while(len>0) {
		if( line[len-1] <= ' ' && line[len-1]>0  ) {
			len-- ;
		}
		else {
			break;
		}
	}
	if( len>=0 ) line[len]='\0' ;
}

char * str_skipspace(char *line)
{
	while (*line >0 && *line<=' ' )
		line++;
	return line;
}

int savetxtfile(char *filename, array <string> & strlist )
{
	FILE *sfile ;
	int i;
	sfile = fopen(filename, "w");
	if (sfile == NULL) {		//      can't open file
		return 0;
	}
	for (i = 0; i < strlist.size(); i++) {
		fputs(strlist[i].getstring(), sfile);
		fputs("\n", sfile);
	}
	fclose(sfile);
	return strlist.size();
}

int readtxtfile(char *filename, array <string> & strlist)
{
	FILE *rfile;
	char buffer[1024];
	string str ;
	rfile = fopen(filename, "r");
	if (rfile == NULL) {
		return 0;
	}
	while (fgets(buffer, sizeof(buffer), rfile)) {
		str_trimtail(buffer);
		str=buffer ;
		strlist.add(str);
	}
	fclose(rfile);
	return strlist.size();
}
