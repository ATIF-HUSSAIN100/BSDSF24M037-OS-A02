#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_DIR     "\033[0;34m"
#define COLOR_EXEC    "\033[0;32m"
#define COLOR_ARCHIVE "\033[0;31m"
#define COLOR_LINK    "\033[0;35m"
#define COLOR_SPECIAL "\033[0;7m"

enum DisplayMode { MODE_VERTICAL, MODE_HORIZONTAL, MODE_LONG };

typedef struct {
    char **names;
    int    count, capacity;
    size_t max_len;
} FileList;

static void fl_init(FileList *fl){fl->names=NULL;fl->count=0;fl->capacity=0;fl->max_len=0;}
static void fl_add(FileList *fl,const char *n){
    if(fl->count>=fl->capacity){
        fl->capacity=fl->capacity?fl->capacity*2:16;
        fl->names=realloc(fl->names,fl->capacity*sizeof(char*));
        if(!fl->names){perror("realloc");exit(1);}
    }
    fl->names[fl->count]=strdup(n);
    size_t l=strlen(n); if(l>fl->max_len)fl->max_len=l;
    fl->count++;
}
static void fl_free(FileList *fl){
    for(int i=0;i<fl->count;i++)free(fl->names[i]);
    free(fl->names);fl->names=NULL;fl->count=fl->capacity=0;fl->max_len=0;
}
static int cmp_strings(const void*a,const void*b){
    return strcmp(*(const char*const*)a,*(const char*const*)b);
}
static int term_width(void){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0&&ws.ws_col>0)return ws.ws_col;
    return 80;
}
static void mode_to_string(mode_t m,char*b){
    b[0]=S_ISDIR(m)?'d':S_ISLNK(m)?'l':S_ISCHR(m)?'c':S_ISBLK(m)?'b':
         S_ISFIFO(m)?'p':S_ISSOCK(m)?'s':'-';
    b[1]=(m&S_IRUSR)?'r':'-'; b[2]=(m&S_IWUSR)?'w':'-'; b[3]=(m&S_IXUSR)?'x':'-';
    b[4]=(m&S_IRGRP)?'r':'-'; b[5]=(m&S_IWGRP)?'w':'-'; b[6]=(m&S_IXGRP)?'x':'-';
    b[7]=(m&S_IROTH)?'r':'-'; b[8]=(m&S_IWOTH)?'w':'-'; b[9]=(m&S_IXOTH)?'x':'-';
    if(m&S_ISUID)b[3]=(b[3]=='x')?'s':'S';
    if(m&S_ISGID)b[6]=(b[6]=='x')?'s':'S';
    if(m&01000)b[9]=(b[9]=='x')?'t':'T';
    b[10]='\0';
}
static int is_archive(const char*n){
    const char*e[]={".tar",".gz",".tgz",".zip",".bz2",".xz",NULL};
    size_t nl=strlen(n);
    for(int i=0;e[i];i++){size_t el=strlen(e[i]);
        if(nl>=el&&strcmp(n+nl-el,e[i])==0)return 1;}
    return 0;
}
static const char*color_for(const char*fp,const char*n){
    struct stat st;
    if(lstat(fp,&st)==-1)return NULL;
    if(S_ISLNK(st.st_mode))return COLOR_LINK;
    if(S_ISDIR(st.st_mode))return COLOR_DIR;
    if(S_ISCHR(st.st_mode)||S_ISBLK(st.st_mode)||S_ISSOCK(st.st_mode)||S_ISFIFO(st.st_mode))
        return COLOR_SPECIAL;
    if(st.st_mode&(S_IXUSR|S_IXGRP|S_IXOTH))return COLOR_EXEC;
    if(is_archive(n))return COLOR_ARCHIVE;
    return NULL;
}
static void print_name(const char*d,const char*n,int pad,int col){
    char fp[PATH_MAX]; snprintf(fp,sizeof fp,"%s/%s",d,n);
    const char*c=col?color_for(fp,n):NULL;
    int p=pad-(int)strlen(n); if(p<0)p=0;
    if(c)printf("%s%s%s",c,n,COLOR_RESET); else printf("%s",n);
    for(int i=0;i<p;i++)putchar(' ');
}
static void print_long_entry(const char*d,const char*n,int col){
    char fp[PATH_MAX]; snprintf(fp,sizeof fp,"%s/%s",d,n);
    struct stat st; if(lstat(fp,&st)==-1){perror("lstat");return;}
    char perm[11]; mode_to_string(st.st_mode,perm);
    struct passwd*pw=getpwuid(st.st_uid);
    struct group*gr=getgrgid(st.st_gid);
    char tb[64]; struct tm*tm=localtime(&st.st_mtime);
    strftime(tb,sizeof tb,"%b %e %H:%M",tm);
    printf("%s %3lu %-8s %-8s %8ld %s ",perm,(unsigned long)st.st_nlink,
        pw?pw->pw_name:"?",gr?gr->gr_name:"?",(long)st.st_size,tb);
    const char*c=col?color_for(fp,n):NULL;
    if(c)printf("%s%s%s",c,n,COLOR_RESET); else printf("%s",n);
    if(S_ISLNK(st.st_mode)){char t[PATH_MAX];ssize_t r=readlink(fp,t,sizeof t-1);
        if(r>=0){t[r]='\0';printf(" -> %s",t);}}
    putchar('\n');
}
static void print_vertical(const char*d,FileList*fl,int col){
    if(!fl->count)return;
    int tw=term_width(), cw=(int)fl->max_len+2;
    int nc=tw/cw; if(nc<1)nc=1;
    int nr=(fl->count+nc-1)/nc;
    for(int r=0;r<nr;r++){
        for(int c=0;c<nc;c++){
            int i=c*nr+r; if(i>=fl->count)continue;
            int last=(c==nc-1)||(c*nr+r>=fl->count-nr);
            print_name(d,fl->names[i],last?0:cw,col);
        }
        putchar('\n');
    }
}
static void print_horizontal(const char*d,FileList*fl,int col){
    if(!fl->count)return;
    int tw=term_width(), cw=(int)fl->max_len+2, pos=0;
    for(int i=0;i<fl->count;i++){
        if(pos+cw>tw&&pos>0){putchar('\n');pos=0;}
        print_name(d,fl->names[i],i==fl->count-1?0:cw,col);
        pos+=cw;
    }
    putchar('\n');
}
static void print_long(const char*d,FileList*fl,int col){
    for(int i=0;i<fl->count;i++)print_long_entry(d,fl->names[i],col);
}
static void display(const char*d,FileList*fl,enum DisplayMode m,int col){
    switch(m){
        case MODE_LONG:print_long(d,fl,col);break;
        case MODE_HORIZONTAL:print_horizontal(d,fl,col);break;
        default:print_vertical(d,fl,col);break;
    }
}
static int read_entries(const char*d,FileList*fl){
    DIR*dir=opendir(d); if(!dir){perror(d);return -1;}
    struct dirent*e;
    while((e=readdir(dir))!=NULL){
        if(e->d_name[0]=='.')continue;
        fl_add(fl,e->d_name);
    }
    closedir(dir);
    qsort(fl->names,fl->count,sizeof(char*),cmp_strings);
    return 0;
}
static void do_ls(const char*d,enum DisplayMode m,int rec,int col){
    FileList fl; fl_init(&fl);
    if(read_entries(d,&fl)==-1){fl_free(&fl);return;}
    if(rec)printf("%s:\n",d);
    display(d,&fl,m,col);
    if(rec){
        for(int i=0;i<fl.count;i++){
            char fp[PATH_MAX]; snprintf(fp,sizeof fp,"%s/%s",d,fl.names[i]);
            struct stat st;
            if(lstat(fp,&st)==-1)continue;
            if(!S_ISDIR(st.st_mode))continue;
            printf("\n");
            do_ls(fp,m,rec,col);
        }
    }
    fl_free(&fl);
}
int main(int argc,char*argv[]){
    enum DisplayMode m=MODE_VERTICAL;
    int rec=0, col=isatty(STDOUT_FILENO), opt;
    while((opt=getopt(argc,argv,"lxR"))!=-1){
        switch(opt){
            case 'l':m=MODE_LONG;break;
            case 'x':m=MODE_HORIZONTAL;break;
            case 'R':rec=1;break;
            default:fprintf(stderr,"Usage: %s [-l] [-x] [-R] [dir]\n",argv[0]);return 1;
        }
    }
    if(optind>=argc)do_ls(".",m,rec,col);
    else for(int i=optind;i<argc;i++)do_ls(argv[i],m,rec,col);
    return 0;
}