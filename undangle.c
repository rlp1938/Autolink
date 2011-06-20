/* undangle.c - recursively search a user input path and if any
 * dangling symlinks are found, remove them
*/
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>


void dohelp(int forced);
void recursedir(const char *path);
char *abspath(const char *path);
char *jointopath(const char *existing, const char *tail);

char *helpmsg =
"\nUsage: undangle [option] path_to_search.\n"
"   Options:\n"
"   h - produce this help message.\n"
;


int main(int argc, char **argv)
{
	struct stat sb;
	char *path;
	if (argc != 2) dohelp(EXIT_FAILURE);
	if (strcmp("-h", argv[1]) == 0) dohelp(EXIT_SUCCESS);
	if ((stat(argv[1], &sb)) == -1) {
		perror(argv[1]);
		exit(EXIT_FAILURE);
	} // if((stat...
	if (!(S_ISDIR(sb.st_mode))) {
		fprintf(stderr, "'%s' is not a directory\n", argv[1]);
		exit(EXIT_FAILURE);
	} // if(!(S_ISDIR...
	// ok the input path is a dir but it may relative or absolute
	// absolute start with '/' and can be processed as is. All else
	// must be converted to absolute paths.
	path = argv[1];
	path = abspath(path);
	recursedir(path);
	free(path);
	return 0;
}// main()

void recursedir(const char *path)
{
	struct dirent *rd;
	struct stat sb;
	DIR *dp = opendir(path);
	if ((chdir(path)) == -1) {
		perror("chdir()");
		exit(EXIT_FAILURE);
	} // if()
	if (!(dp)) {
		perror(path);
		exit(EXIT_FAILURE);
	} // if (!(dp))
	while ((rd = readdir(dp))) {
		char *cp;
		if ((strcmp("..", rd->d_name)) == 0) continue;
		if ((strcmp(".", rd->d_name)) == 0) continue;
		switch(rd->d_type) { // What if it's a DIR and a symlink?
			case DT_DIR:
			cp = jointopath(path, rd->d_name);
			if ((chdir(cp)) == -1) {
				perror("chdir");
				exit(EXIT_FAILURE);
			} // if()
			recursedir(cp);
			// now change dir back
			if ((chdir(path)) == -1) {
				perror("chdir");
				exit(EXIT_FAILURE);
			} // if()
			free(cp);	
			break;
			case DT_LNK:
			if ((stat(rd->d_name, &sb)) == -1) {// it's a dangling link
				if ((unlink(rd->d_name)) == -1) {
					perror("unlink");
					exit(EXIT_FAILURE);
				} // if((unlink...
			} // if()
			break;
			default:	// not interested in anything else
			break;
		} // switch(..
	} // while(rd)
	closedir(dp);	
} // recursedir()

void dohelp(int forced)
{   
    fputs(helpmsg, stderr);
    exit(forced);
} // dohelp()

char *abspath(const char *path)
{ /* converts path to an absolute path
	* the returned value must be free()'d
	* when done with.
*/
	char pwd[PATH_MAX+1];
	char *ret, *work, *result;
	ret = strdup(path);
	if (!(ret)) {
		perror(path);
		exit(EXIT_FAILURE);
	} // if(!(ret...
	if (path[0] == '/') { // I have an absolute path
		return ret;
	} // if(path...
	// well I might have something like this '../../something'
	work = ret;
	while ((strncmp("../", work, 3)) == 0) {
		work += 3;
		if((chdir("../")) == -1) {
			perror("chdir");
			exit(EXIT_FAILURE);
		} // if((chdir...
	} // while((strn...
	// or maybe something like this './something'
	while ((strncmp("./", work, 2)) == 0) {
		work += 2;
	} // while((strn...
	if (!(getcwd(pwd, PATH_MAX))) {
		perror("getcwd");
		exit(EXIT_FAILURE);
	} // if(!(getcwd...
	result = jointopath(pwd, work);
	free(ret);
	return result;
} // abspath()

char *jointopath(const char *existing, const char *tail)
{
    static char *newpath;
    newpath = malloc(strlen(existing) + strlen(tail) + 2);
    if (!(newpath)) {
        perror("malloc");
        exit(EXIT_FAILURE);
    } // if()
    strcpy(newpath, existing);
    strcat(newpath, "/");
    strcat(newpath, tail);
    return newpath;
} // jointopath()
