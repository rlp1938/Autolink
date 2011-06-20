/* autolink.c - given 2 dirs make links in the second dir to the 
 * content of the first dir. It does this recursively.
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
void searchdir(const char *dirpath, char **exclv, FILE *fpo);
char *jointopath(const char *existing, const char *tail);
void insertlink(const char *linktopath, const char *linkfrompath,
                int lentopath, int hardlink);

const int dirperms = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
char *helpmsg =
"\nUsage: autolink [option] source_dir link_to_dir\n"
"   Options:\n"
"   h - produce this help message.\n"
"   l - links to be hard links.\n"
"   s - links to be symlinks, this is the default.\n"
"   e - pathname to exclude. You may use this option for many paths.\n"
;

int main(int argc, char **argv)
{
    extern char *optarg;
    extern int optind, opterr, optopt;
    int dohardlink, opt;
    char *dirfrom, *dirlink;
    struct stat sb;
    int exclc, exclmax, len, result;
    char **exclv;
    FILE *fp;
    char buf[PATH_MAX+1];
    
    dohardlink = 0; // default is symlink
    exclmax = 10;
    exclv = malloc(sizeof(char **) * (exclmax + 1));
    if (!(exclv)) {
        perror("memory");
        exit(EXIT_FAILURE);
    } // if()
    for(exclc = 0; exclc <= exclmax; exclc++)
        exclv[exclc] = NULL;
    exclc = 0;
    while ((opt = getopt(argc, argv, ":hlse:")) != -1) {
        int len;
        switch (opt) {
            case 'h':
            dohelp(EXIT_SUCCESS);
            break;
            case 'l':
            dohardlink = 1;
            break;
            case 's':
            dohardlink = 0;
            break;
            case 'e':   // add to the list of excluded dirs
            if (exclc == exclmax) { // get more memory
                int saveit = exclc;
                exclmax += exclmax;
                exclv = realloc(exclv, exclmax);
                if (!(exclv)) {
                    perror("memory");
                    exit(EXIT_FAILURE);
                } // if (exclv)
                for( ;exclc <= exclmax; exclc++)
                    exclv[exclc] = NULL;
                exclc = saveit;
            } // if(exclc...)
            exclv[exclc] = strdup(optarg);
            if (!(exclv[exclc])) {
                perror("strdup");
                exit(EXIT_FAILURE);
            } // if()
            if ((stat(optarg, &sb)) == -1) {
                fprintf(stderr, "Could not stat: '%s'\n", optarg);
                perror(optarg);
                exit(EXIT_FAILURE);
            }//if(stat...
            if (!(S_ISDIR(sb.st_mode))) {
                fprintf(stderr, "'%s' is not a directory\n", optarg);
                exit(EXIT_FAILURE);
            } // if(!(S_ISDIR...
            // trim off any trailing '/' on the exclude path
            len = strlen(exclv[exclc]);
            if (exclv[exclc][len-1] == '/')
                exclv[exclc][len-1] = '\0';
            exclc++;
            break;
            case ':':
            fprintf(stderr, "Option '%c' requires an argument.\n",
                        optopt);
            dohelp(EXIT_FAILURE);
            break;
            case '?':
            fprintf(stderr, "\nInvalid option '%c'.\n", optopt);
            dohelp(EXIT_FAILURE);
            break;
            default:
            fputs("This condition should never happen\n", stderr);
            exit(EXIT_FAILURE);
            break;
        } // switch()
    } // while()
    dirfrom = argv[optind];
    if ((stat(dirfrom, &sb)) == -1) {
        perror(dirfrom);
        dohelp(EXIT_FAILURE);
    } // if(!(S_ISDIR)) 
    if (!(S_ISDIR(sb.st_mode))) {
        fprintf(stderr, "'%s' is not a directory\n", dirfrom);
        dohelp(EXIT_FAILURE);
    } // if()
    // trim any trailing '/' from dirfrom
    len = strlen(dirfrom);
    if (dirfrom[len-1] == '/')
        dirfrom[len-1] = '\0';
    // set up the dir where we are to insert the links
    optind++;
    dirlink = argv[optind];
    if ((stat(dirlink, &sb)) == -1) {
        perror(dirlink);
        dohelp(EXIT_FAILURE);
    } // if(!(S_ISDIR)) 
    if (!(S_ISDIR(sb.st_mode))) {
        fprintf(stderr, "'%s' is not a directory\n", dirlink);
        dohelp(EXIT_FAILURE);
    } // if()
    // trim any trailing '/' from dirfrom
    len = strlen(dirlink);
    if (dirlink[len-1] == '/')
        dirlink[len-1] = '\0';

    // Now recursively search dirfrom and send the result to 'al_from'
    fp = fopen("al_from", "w");
    if (!(fp)) {
        perror("al_from");
        exit(EXIT_FAILURE);
    } // if(!(..
    searchdir(dirfrom, exclv, fp);
    sync();
    fclose(fp);
    // sort the result
    result = system("/usr/bin/sort al_from > al_from.srt");
    if (result == -1) {
        perror("system");
        exit(EXIT_FAILURE);
    } // if()
    sync();
    // rename my sorted file
    result = rename("al_from.srt", "al_from");
    if (result == -1) {
        perror("rename");
        exit(EXIT_FAILURE);
    } // if()
    // now open our sorted and filtered list in 'al_from'
    fp = fopen("al_from", "r");
    if (!(fp)) {
        perror("open");
        exit(EXIT_FAILURE);
    } // if()
    while((fgets(buf, PATH_MAX, fp))) {
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0'; // goodbye newline
        if ((stat(buf, &sb)) == -1) { // corrupted file data ???
            perror(buf);
            exit(EXIT_FAILURE);
        } // if()
        insertlink(buf, dirlink, strlen(dirlink), dohardlink);
    } // while(fgets...
    return EXIT_SUCCESS;
}// main()

void dohelp(int forced)
{   
    fputs(helpmsg, stderr);
    exit(forced);
} // dohelp()

void searchdir(const char *dirpath, char **exclv, FILE *fpo)
{
    struct dirent *dep;
    DIR *dp = opendir(dirpath);
    if (!(dp)) {
        perror(dirpath);
        exit(EXIT_FAILURE);
    } // if()
    dep = malloc(sizeof(struct dirent));
    if (!(dep)) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }// if()
    while ((dep = readdir(dp))){
        char *cp = NULL;
        int i;
        switch(dep->d_type){
            case DT_DIR:    // a dir so print the value of it
            // unless it's ".." or "."
            if ((strcmp(dep->d_name, "..") == 0)) break;
            if ((strcmp(dep->d_name, ".") == 0)) break;
            cp = jointopath(dirpath, dep->d_name);
            // And unless it's in one of our excluded paths
            for (i=0; exclv[i]; i++) {
                int len = strlen(exclv[i]);
                if ((strncmp(exclv[i], cp, len)) == 0) goto exitsw;
            } // for(i...
            if ((fprintf(fpo, "%s\n", cp)) < 0) {
                perror(cp);
            } // if()
            searchdir(cp, exclv, fpo);
            break;
            case DT_LNK:
            cp = jointopath(dirpath, dep->d_name);
            fprintf(fpo, "%s\n", cp);
            break;
            case DT_REG:
            cp = jointopath(dirpath, dep->d_name);
            if ((fprintf(fpo, "%s\n", cp)) < 0) {
                perror(cp);
            } // if()
            break;
            default:
            cp = jointopath(dirpath, dep->d_name);
            fprintf(stderr, "unexpected item found: %s\n", cp);
            break;
        } // switch()
exitsw:
        free(cp);
    } // while()
} // searchdir()

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

void insertlink(const char *linktopath, const char *linkfrompath,
                int lentopath, int hardlink)
{
    /* linktopath is the total path as read from the list of source
     * paths. lentopath is the length of the root of the linktopath.
     * linkfrompath is the original root of the link from path.
     * ltptail is the variable part of the linkto path and I concatenate
     * that onto the linkfrompath to see if it exists; if not create it.
    */ 
    char *ltptail, *lfp;
    struct stat sb;
    ltptail = linktopath + lentopath + 2;   // point past the '/'
    lfp = jointopath(linkfrompath, ltptail);
    // at this stage I don't care what lfp is I just need to know if
    // it exists or not.
    if ((stat(lfp, &sb)) == -1) { // non existent, create it
		// so now I do need to know what to create
		(void)stat(linktopath, &sb);
		if (S_ISREG(sb.st_mode)) {	// it's a file
			if (hardlink) {	// make hardlink
				if ((link(linktopath, lfp)) == -1) {
					perror(lfp);
					exit(EXIT_FAILURE);
				} // if((link...
			} else { // make symlink
				if ((symlink(linktopath, lfp)) == -1) {
					perror(lfp);
					exit(EXIT_FAILURE);
				} // if((symlink...
			} // if (hardlink)
		} else if (S_ISDIR(sb.st_mode)) {	// it's a dir
			if ((mkdir(lfp, dirperms)) == -1) {
				perror(lfp);
				exit(EXIT_FAILURE);
			} // if((mkdir..
		} else {	// It's something unexpected
			fprintf(stderr,"An unexpected event has happened\n%s\n%s\n",
						linktopath, lfp);
			exit(EXIT_FAILURE);
		} // if(S_....
    } // if (( stat...
    free (lfp);
} // insertlink()
