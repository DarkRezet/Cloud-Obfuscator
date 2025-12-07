#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <Windows.h>
#include <wincrypt.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#define KL 32
static unsigned char S[KL];

static void sha256(const unsigned char*data,size_t len,unsigned char*out){
#ifdef _WIN32
    HCRYPTPROV hProv=0;
    HCRYPTHASH hHash=0;
    CryptAcquireContext(&hProv,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv,CALG_SHA_256,0,0,&hHash);
    CryptHashData(hHash,data,(DWORD)len,0);
    DWORD l=32;
    CryptGetHashParam(hHash,HP_HASHVAL,out,&l,0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv,0);
#else
    FILE*f=popen("sha256sum","w");
    (void)f; 
    for(int i=0;i<32;i++)out[i]=i;
#endif
}

static int load(){
    FILE*f=fopen("secret.bin","rb");
    if(f){fread(S,1,KL,f);fclose(f);return 0;}
#ifdef _WIN32
    HCRYPTPROV h;
    CryptAcquireContext(&h,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT);
    CryptGenRandom(h,KL,S);
    CryptReleaseContext(h,0);
#else
    int fd=open("/dev/urandom",O_RDONLY);
    read(fd,S,KL);
    close(fd);
#endif
    f=fopen("secret.bin","wb");
    fwrite(S,1,KL,f);
    fclose(f);
    return 0;
}

static PyObject* gk(PyObject*s,PyObject*a){
    load();
    return PyBytes_FromStringAndSize((char*)S,KL);
}

static PyObject* hwid(){
#ifdef _WIN32
    char b[260];DWORD l=260;
    GetComputerNameA(b,&l);
    return PyBytes_FromStringAndSize(b,l);
#else
    char b[256];
    FILE*f=fopen("/etc/machine-id","rb");
    size_t r=fread(b,1,256,f);
    fclose(f);
    return PyBytes_FromStringAndSize(b,r);
#endif
}

static PyObject* dk(PyObject*s,PyObject*a){
    PyObject*so;
    PyArg_ParseTuple(a,"S",&so);
    char*sb;Py_ssize_t sl;
    PyBytes_AsStringAndSize(so,&sb,&sl);
    load();
    PyObject*h=hwid();
    char*hb;Py_ssize_t hl;
    PyBytes_AsStringAndSize(h,&hb,&hl);
    size_t t=KL+hl+sl;
    unsigned char*buf=malloc(t);
    memcpy(buf,S,KL);
    memcpy(buf+KL,hb,hl);
    memcpy(buf+KL+hl,sb,sl);
    unsigned char out[32];
    sha256(buf,t,out);
    free(buf);
    Py_DECREF(h);
    return PyBytes_FromStringAndSize((char*)out,32);
}

static PyMethodDef M[]={
 {"get_key",gk,METH_NOARGS,0},
 {"derive_key",dk,METH_VARARGS,0},
 {0,0,0,0}
};

static struct PyModuleDef md={
 PyModuleDef_HEAD_INIT,"keymodule",0,-1,M
};

PyMODINIT_FUNC PyInit_keymodule(){
    return PyModule_Create(&md);
}