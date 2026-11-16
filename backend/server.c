/*
 CampusFix - C backend (Windows / MinGW GCC)
 Academic demonstration server. Run this program from the CampusFix project root.
 Demo admin credentials: admin / admin123
 NOTE: This is a classroom prototype, not a production web server. Passwords are
 stored as plain text for demonstration only. Use dummy passwords, never real ones.
*/
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSESOCKET closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define CLOSESOCKET close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define PORT 8080
#define REQ_SIZE 65536
#define MAX_COMPLAINTS 3000
#define STUDENTS_FILE "data/students.txt"
#define COMPLAINTS_FILE "data/complaints.txt"

typedef struct {
    int id;
    char usn[32], category[32], location[120], description[520], status[24], created[32];
} Complaint;

static Complaint complaints[MAX_COMPLAINTS];
static int complaint_count = 0;
static int next_id = 1;

static void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}
static void json_escape(const char *src, char *dst, size_t cap) {
    size_t j=0;
    for(size_t i=0; src[i] && j+2<cap; i++) {
        unsigned char c=(unsigned char)src[i];
        if(c=='"' || c=='\\') { dst[j++]='\\'; dst[j++]=(char)c; }
        else if(c=='\n') { dst[j++]='\\'; dst[j++]='n'; }
        else if(c=='\r') { dst[j++]='\\'; dst[j++]='r'; }
        else if(c>=32) dst[j++]=(char)c;
    }
    dst[j]='\0';
}
static void get_json_value(const char *body, const char *key, char *out, size_t cap) {
    char needle[80]; snprintf(needle,sizeof(needle),"\"%s\"",key);
    const char *p=strstr(body,needle); out[0]='\0'; if(!p)return;
    p=strchr(p,':'); if(!p)return; p++;
    while(*p && isspace((unsigned char)*p))p++;
    if(*p!='"')return; p++;
    size_t j=0;
    while(*p && *p!='"' && j+1<cap) {
        if(*p=='\\' && p[1]) { p++; if(*p=='n')out[j++]='\n'; else out[j++]=*p; p++; }
        else out[j++]=*p++;
    }
    out[j]='\0';
}
static int get_json_int(const char *body, const char *key) {
    char needle[80]; snprintf(needle,sizeof(needle),"\"%s\"",key);
    const char *p=strstr(body,needle); if(!p)return -1;
    p=strchr(p,':'); if(!p)return -1; return atoi(p+1);
}
static void send_all(socket_t client, const char *data, size_t len) {
    while(len>0) {
        int sent=(int)send(client,data,(int)len,0);
        if(sent<=0) return;
        data+=sent; len-=(size_t)sent;
    }
}
static void send_response(socket_t client, const char *status, const char *type, const char *body) {
    char header[512]; size_t len=strlen(body);
    snprintf(header,sizeof(header),"HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nConnection: close\r\n\r\n",status,type,(unsigned long)len);
    send_all(client,header,strlen(header)); send_all(client,body,len);
}
static void send_json(socket_t c, int ok, const char *message) {
    char esc[600], body[800]; json_escape(message,esc,sizeof(esc));
    snprintf(body,sizeof(body),"{\"success\":%s,\"message\":\"%s\"}",ok?"true":"false",esc);
    send_response(c,ok?"200 OK":"400 Bad Request","application/json; charset=utf-8",body);
}
static void load_complaints(void) {
    FILE *f=fopen(COMPLAINTS_FILE,"r"); if(!f)return;
    char line[1200];
    while(fgets(line,sizeof(line),f) && complaint_count<MAX_COMPLAINTS) {
        trim_newline(line);
        Complaint c; memset(&c,0,sizeof(c));
        char *parts[7]; int n=0; char *p=line;
        while(n<7) { parts[n++]=p; char *bar=strchr(p,'|'); if(!bar)break; *bar='\0'; p=bar+1; }
        if(n<7)continue;
        c.id=atoi(parts[0]);
        snprintf(c.usn,sizeof(c.usn),"%s",parts[1]);
        snprintf(c.category,sizeof(c.category),"%s",parts[2]);
        snprintf(c.location,sizeof(c.location),"%s",parts[3]);
        snprintf(c.description,sizeof(c.description),"%s",parts[4]);
        snprintf(c.status,sizeof(c.status),"%s",parts[5]);
        snprintf(c.created,sizeof(c.created),"%s",parts[6]);
        complaints[complaint_count++]=c; if(c.id>=next_id)next_id=c.id+1;
    }
    fclose(f);
}
static void save_complaints(void) {
    FILE *f=fopen(COMPLAINTS_FILE,"w"); if(!f)return;
    for(int i=0;i<complaint_count;i++) {
        Complaint *c=&complaints[i];
        fprintf(f,"%d|%s|%s|%s|%s|%s|%s\n",c->id,c->usn,c->category,c->location,c->description,c->status,c->created);
    }
    fclose(f);
}
static int student_exists(const char *usn) {
    FILE *f=fopen(STUDENTS_FILE,"r"); if(!f)return 0;
    char line[400], saved[32]; int found=0;
    while(fgets(line,sizeof(line),f)) { if(sscanf(line,"%31[^|]",saved)==1 && strcmp(saved,usn)==0){found=1;break;} }
    fclose(f); return found;
}
static int check_student(const char *usn,const char *password,char *name,size_t namecap) {
    FILE *f=fopen(STUDENTS_FILE,"r"); if(!f)return 0;
    char line[400], su[32],email[110],pw[64],nm[64]; int found=0;
    while(fgets(line,sizeof(line),f)) {
        trim_newline(line);
        if(sscanf(line,"%31[^|]|%63[^|]|%109[^|]|%63[^\n]",su,nm,email,pw)==4 &&
           strcmp(su,usn)==0 && strcmp(pw,password)==0) {
            snprintf(name,namecap,"%s",nm); found=1; break;
        }
    }
    fclose(f); return found;
}
static void handle_register(socket_t c,const char *body) {
    char name[64],usn[32],email[110],password[64];
    get_json_value(body,"name",name,sizeof(name)); get_json_value(body,"usn",usn,sizeof(usn));
    get_json_value(body,"email",email,sizeof(email)); get_json_value(body,"password",password,sizeof(password));
    if(!name[0]||!usn[0]||!email[0]||!password[0]){send_json(c,0,"Please fill all registration fields.");return;}
    if(strpbrk(name,"|\r\n")||strpbrk(usn,"|\r\n")||strpbrk(email,"|\r\n")||strpbrk(password,"|\r\n")){send_json(c,0,"Do not use | or line breaks in the fields.");return;}
    if(student_exists(usn)){send_json(c,0,"This USN is already registered. Please log in.");return;}
    FILE *f=fopen(STUDENTS_FILE,"a"); if(!f){send_json(c,0,"Cannot open student data file. Check the data folder.");return;}
    fprintf(f,"%s|%s|%s|%s\n",usn,name,email,password); fclose(f);
    send_json(c,1,"Registration successful. You can now log in.");
}
static void handle_login(socket_t c,const char *body) {
    char usn[32],password[64],name[64],escname[140],escusn[80],out[300];
    get_json_value(body,"usn",usn,sizeof(usn)); get_json_value(body,"password",password,sizeof(password));
    if(!check_student(usn,password,name,sizeof(name))){send_json(c,0,"Incorrect USN or password.");return;}
    json_escape(name,escname,sizeof(escname)); json_escape(usn,escusn,sizeof(escusn));
    snprintf(out,sizeof(out),"{\"success\":true,\"message\":\"Login successful.\",\"student\":{\"name\":\"%s\",\"usn\":\"%s\"}}",escname,escusn);
    send_response(c,"200 OK","application/json; charset=utf-8",out);
}
static void handle_create_complaint(socket_t c,const char *body) {
    char usn[32],category[32],location[120],description[520];
    get_json_value(body,"usn",usn,sizeof(usn)); get_json_value(body,"category",category,sizeof(category));
    get_json_value(body,"location",location,sizeof(location)); get_json_value(body,"description",description,sizeof(description));
    if(!usn[0]||!category[0]||!location[0]||!description[0]){send_json(c,0,"Please fill all complaint fields.");return;}
    if(!student_exists(usn)){send_json(c,0,"Student account not found. Please register first.");return;}
    if(strpbrk(usn,"|\r\n")||strpbrk(category,"|\r\n")||strpbrk(location,"|\r\n")||strpbrk(description,"|\r\n")){send_json(c,0,"Please remove | or line breaks from the fields.");return;}
    if(complaint_count>=MAX_COMPLAINTS){send_json(c,0,"Complaint storage is full for this demo.");return;}
    Complaint *x=&complaints[complaint_count++]; memset(x,0,sizeof(*x));
    x->id=next_id++; snprintf(x->usn,sizeof(x->usn),"%s",usn); snprintf(x->category,sizeof(x->category),"%s",category);
    snprintf(x->location,sizeof(x->location),"%s",location); snprintf(x->description,sizeof(x->description),"%s",description);
    snprintf(x->status,sizeof(x->status),"Pending");
    time_t now=time(NULL); struct tm *t=localtime(&now);
    if(t)strftime(x->created,sizeof(x->created),"%d-%m-%Y %H:%M",t); else strcpy(x->created,"Unknown");
    save_complaints(); char msg[100]; snprintf(msg,sizeof(msg),"Complaint #%d submitted successfully.",x->id); send_json(c,1,msg);
}
static void handle_list_complaints(socket_t c,const char *path) {
    char filter[64]=""; const char *q=strstr(path,"usn=");
    if(q){q+=4;size_t i=0;while(q[i]&&q[i]!='&'&&i+1<sizeof(filter)){filter[i]=q[i];i++;}filter[i]='\0';}
    char *out=(char*)malloc(REQ_SIZE*4); if(!out){send_json(c,0,"Server memory error.");return;}
    size_t used=0; used+=(size_t)snprintf(out+used,REQ_SIZE*4-used,"{\"success\":true,\"complaints\":[");
    int first=1;
    for(int i=0;i<complaint_count;i++){
        Complaint *x=&complaints[i]; if(filter[0]&&strcmp(filter,x->usn)!=0)continue;
        char usn[80],cat[80],loc[260],desc[1100],status[80],created[80];
        json_escape(x->usn,usn,sizeof(usn));json_escape(x->category,cat,sizeof(cat));json_escape(x->location,loc,sizeof(loc));
        json_escape(x->description,desc,sizeof(desc));json_escape(x->status,status,sizeof(status));json_escape(x->created,created,sizeof(created));
        int n=snprintf(out+used,REQ_SIZE*4-used,"%s{\"id\":%d,\"usn\":\"%s\",\"category\":\"%s\",\"location\":\"%s\",\"description\":\"%s\",\"status\":\"%s\",\"created\":\"%s\"}",first?"":",",x->id,usn,cat,loc,desc,status,created);
        if(n<0||(size_t)n>=REQ_SIZE*4-used)break; used+=(size_t)n; first=0;
    }
    snprintf(out+used,REQ_SIZE*4-used,"]}");
    send_response(c,"200 OK","application/json; charset=utf-8",out); free(out);
}
static void handle_update_status(socket_t c,const char *body) {
    int id=get_json_int(body,"id"); char status[32]; get_json_value(body,"status",status,sizeof(status));
    if(strcmp(status,"Pending")&&strcmp(status,"In Progress")&&strcmp(status,"Resolved")){send_json(c,0,"Invalid status.");return;}
    for(int i=0;i<complaint_count;i++)if(complaints[i].id==id){strcpy(complaints[i].status,status);save_complaints();send_json(c,1,"Complaint status updated.");return;}
    send_json(c,0,"Complaint ID not found.");
}
static const char *mime_type(const char *path) {
    const char *dot=strrchr(path,'.'); if(!dot)return "text/plain";
    if(strcmp(dot,".html")==0)return "text/html; charset=utf-8";
    if(strcmp(dot,".css")==0)return "text/css; charset=utf-8";
    if(strcmp(dot,".js")==0)return "application/javascript; charset=utf-8";
    if(strcmp(dot,".png")==0)return "image/png";
    if(strcmp(dot,".jpg")==0||strcmp(dot,".jpeg")==0)return "image/jpeg";
    return "application/octet-stream";
}
static void serve_file(socket_t c,const char *urlpath) {
    char path[260];
    if(strcmp(urlpath,"/")==0)strcpy(path,"frontend/index.html");
    else {
        if(strstr(urlpath,"..")){send_response(c,"400 Bad Request","text/plain","Invalid path.");return;}
        snprintf(path,sizeof(path),"frontend/%s",urlpath[0]=='/'?urlpath+1:urlpath);
    }
    FILE *f=fopen(path,"rb"); if(!f){send_response(c,"404 Not Found","text/plain","File not found.");return;}
    fseek(f,0,SEEK_END); long size=ftell(f); rewind(f);
    if(size<0||size>2*1024*1024){fclose(f);send_response(c,"500 Internal Server Error","text/plain","File too large.");return;}
    char *data=(char*)malloc((size_t)size+1); if(!data){fclose(f);return;}
    size_t got=fread(data,1,(size_t)size,f); fclose(f);
    char header[512]; snprintf(header,sizeof(header),"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",mime_type(path),(unsigned long)got);
    send_all(c,header,strlen(header));send_all(c,data,got);free(data);
}
static void handle_client(socket_t c) {
    char req[REQ_SIZE]; int total=0,n;
    while(total<(int)sizeof(req)-1) {
        n=(int)recv(c,req+total,(int)sizeof(req)-1-total,0); if(n<=0)break; total+=n; req[total]='\0';
        char *end=strstr(req,"\r\n\r\n");
        if(end){int headerlen=(int)(end+4-req);int contentlen=0;char *cl=strstr(req,"Content-Length:");
            if(cl&&cl<end)contentlen=atoi(cl+15);
            if(total>=headerlen+contentlen)break;
        }
    }
    req[total]='\0'; if(total==0)return;
    char method[12]="",path[300]="";sscanf(req,"%11s %299s",method,path);
    char *header_end=strstr(req,"\r\n\r\n"); char *body=header_end?header_end+4:"";
    if(strcmp(method,"OPTIONS")==0){send_response(c,"204 No Content","application/json","");return;}
    if(strcmp(method,"GET")==0){
        if(strcmp(path,"/api/complaints")==0||strncmp(path,"/api/complaints?",15)==0){handle_list_complaints(c,path);return;}
        serve_file(c,path);return;
    }
    if(strcmp(method,"POST")==0){
        if(strcmp(path,"/api/register")==0){handle_register(c,body);return;}
        if(strcmp(path,"/api/login")==0){handle_login(c,body);return;}
        if(strcmp(path,"/api/complaints")==0){handle_create_complaint(c,body);return;}
        if(strcmp(path,"/api/admin-login")==0){
            char u[40],p[80];get_json_value(body,"username",u,sizeof(u));get_json_value(body,"password",p,sizeof(p));
            if(strcmp(u,"admin")==0&&strcmp(p,"admin123")==0){send_json(c,1,"Admin login successful.");return;}
            send_json(c,0,"Incorrect admin username or password.");return;
        }
        if(strcmp(path,"/api/update-status")==0){handle_update_status(c,body);return;}
    }
    send_response(c,"404 Not Found","application/json","{\"success\":false,\"message\":\"Route not found.\"}");
}
int main(void) {
#ifdef _WIN32
    WSADATA wsa; if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){printf("Winsock startup failed.\\n");return 1;}
#endif
    load_complaints();
    socket_t server=socket(AF_INET,SOCK_STREAM,0);
    if(server==INVALID_SOCKET){printf("Could not create server socket.\\n");return 1;}
    int opt=1; setsockopt(server,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));
    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;addr.sin_addr.s_addr=INADDR_ANY;addr.sin_port=htons(PORT);
    if(bind(server,(struct sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){printf("Could not use port %d. Is another server running?\\n",PORT);CLOSESOCKET(server);return 1;}
    if(listen(server,10)==SOCKET_ERROR){printf("Server listen failed.\\n");CLOSESOCKET(server);return 1;}
    printf("\\nCampusFix C server is running.\\nOpen http://localhost:%d in your browser.\\nPress Ctrl+C to stop.\\n",PORT);
    while(1){socket_t client=accept(server,NULL,NULL);if(client==INVALID_SOCKET)continue;handle_client(client);CLOSESOCKET(client);}
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
