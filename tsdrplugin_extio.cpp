﻿#include "tsdrplugin_extio.h"
#include "./ui_tsdrplugin_extio.h"

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <QDebug>
#include <QMessageBox>
#include <thread>
//#include <commctrl.h> #include <process.h> #include <tchar.h>
//#include "resource.h"
#include "rtl-sdr.h"

// GLOBAL TODO, since i am far from mastering c++, and im very comfortable in using C i used a lot of functions that are "buffer unsafe" later we have to change some visibilities, and exchange the functions that handle memory


#define EXTIO_HWTYPE_16B	3

#define MAX_PPM	1000
#define MIN_PPM	-1000
static int ppm_default=0;
//TODO POINTER STUFF
typedef struct sr {
	double value;
	char name[16];
} sr_t;

static sr_t samplerates[] = {
	{  250000.0, "0.25 Msps" },
	{  960000.0, "0.96 Msps" },
	{ 1028571.0, "1.02 Msps" },
	{ 1200000.0, "1.2 Msps" },
	{ 1440000.0, "1.44 Msps" },
	{ 1800000.0, "1.8 Msps" },
	{ 2400000.0, "2.4 Msps" },
	{ 2880000.0, "2.88 Msps"},
	{ 3200000.0, "3.2 Msps" }
};

static int samplerate_default=6; // 2.4 Msps
#define MAXRATE		3200000 
#define MINRATE		900001 

static const char* directS[] = {
	"Disabled",
	"I input",
	"Q input" 
};

static int directS_default=0; // Disabled


static int TunerAGC_default=1;
static int RTLAGC_default=0;
static int HDSDR_AGC=2;
static int OffsetT_default=1;
static int device_default=0;

static int gain_default=0;
static int n_gains;
static int last_gain;
static int *gains;

static int buffer_sizes[] = { //in kBytes
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128,
	256,
	512,
	1024
};

static int buffer_default=6;// 64kBytes

static int buffer_len;

typedef struct {
	char vendor[256], product[256], serial[256];
} device;

static device *connected_devices = NULL;

static rtlsdr_dev_t *dev = NULL;
static int device_count = 0;



void ThreadProc(void * param);

int Start_Thread();
int Stop_Thread();



void RTLSDRCallBack(unsigned char *buf, uint32_t len, void *ctx);
short *short_buf = NULL;

/* ExtIO Callback */
void (* WinradCallBack)(int, int, float, void *) = NULL;
#define WINRAD_SRCHANGE 100
#define WINRAD_LOCHANGE 101
#define WINRAD_ATTCHANGE 125

Ui_TSDRPlugin_ExtIO* handle;
QMessageBox* msgBox;

//HWND h_dialog=NULL;

int pll_locked=0;


TSDRPlugin_ExtIO::TSDRPlugin_ExtIO(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TSDRPlugin_ExtIO)
{
    QMessageBox messageBox;
    ui->setupUi(this);
    handle=this->ui;
    msgBox=&messageBox;

    //InitHW(name, model, tipe);
    //MainCallback();
}

TSDRPlugin_ExtIO::~TSDRPlugin_ExtIO()
{
    delete ui;
}


//bool  LIBRTL_API __stdcall InitHW(char *name, char *model, int& type)

extern "C"
bool InitHW(char *name, char *model, int& type){
    //MessageBox(NULL, TEXT("InitHW"),NULL, MB_OK);


    device_count = rtlsdr_get_device_count();
    if (!device_count){
        reinterpret_cast<QMessageBox *>(msgBox)->setText("No RTLSDR devices found");
        reinterpret_cast<QMessageBox *>(msgBox)->exec();
        qDebug() << "No RTLSDR devices found";
        return false;
    }

    connected_devices = new (std::nothrow) device[device_count];
    for( int i=0; i<device_count;i++)
        rtlsdr_get_device_usb_strings(0, connected_devices[i].vendor, connected_devices[i].product, connected_devices[i].serial);

    strcpy(name,connected_devices[0].vendor);
    strcpy(model,connected_devices[0].product);


    //strcpy and other associated str functions already append the 0 on the last position
    //name[15]=0;
    //model[15]=0;
    //
    //TOCHECK whats the point here?
    type = EXTIO_HWTYPE_16B; /* ExtIO type 16-bit samples */

    return true;
}

extern "C"
//int LIBRTL_API __stdcall GetStatus()
int GetStatus()
{
    /* dummy function */
    return 0;
}

extern "C"
//bool  LIBRTL_API __stdcall OpenHW()
bool OpenHW(){
    //MessageBox(NULL, TEXT("OpenHW"),NULL, MB_OK);
    int r;

    if ( device_default>=device_count) device_default=0;
    r = rtlsdr_open(&dev,device_default);

    if(r < 0) {
//		MessageBox(NULL, TEXT("OpenHW Fudeu"),NULL, MB_OK);
        return false;
    }

    r=rtlsdr_set_sample_rate(dev, samplerates[samplerate_default].value);
    if( r < 0)
        return false;

    /*h_dialog=CreateDialog(hInst, MAKEINTRESOURCE(IDD_RTL_SETTINGS), NULL, (DLGPROC)MainDlgProc);
    ShowWindow(h_dialog,SW_HIDE);

    */

    return true;
}

//TODO
extern "C"
//long LIBRTL_API __stdcall SetHWLO(long freq)
long SetHWLO(long freq)
{
    long r;

    r=rtlsdr_set_center_freq(dev, freq);

    if (r!=pll_locked){
        pll_locked=r;
        /*if (pll_locked==0){

            //Static_SetText(GetDlgItem(h_dialog,IDC_PLL),TEXT("PLL LOCKED"));
        }else{

            //Static_SetText(GetDlgItem(h_dialog,IDC_PLL),TEXT("PLL NOT LOCKED"));
        }
        InvalidateRect(h_dialog, NULL, TRUE);

        UpdateWindow(h_dialog);
        */
    }
    if (r!=0) {
        //MessageBox(NULL, TEXT("PLL not locked!"),TEXT("Error!"), MB_OK|MB_ICONERROR);
        return -1;
    }


    r=rtlsdr_get_center_freq(dev);

    if (r!=freq )
        WinradCallBack(-1,WINRAD_LOCHANGE,0,NULL);

    return 0;
}


extern "C"
//int LIBRTL_API __stdcall StartHW(long freq)
int StartHW(long freq)
{

    if (!dev) return -1;

    short_buf = new (std::nothrow) short[buffer_len];
    if (short_buf==0) {
        //MessageBox(NULL, TEXT("Couldn't Allocate Buffer!"),TEXT("Error!"), MB_OK|MB_ICONERROR);
        return -1;
    }

    if(Start_Thread()<0)
    {
        delete[] short_buf;
        return -1;
    }

    SetHWLO(freq);

    reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->DeviceComboBox->setEnabled(false);
    reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->HFDirectSamplingCombo->setEnabled(false);


    return buffer_len/2;
}

//long LIBRTL_API __stdcall GetHWLO()
extern "C"
long GetHWLO()
{
    static long last_freq=100000000;
    long freq;

    //MessageBox(NULL, TEXT("GetHWLO"),NULL, MB_OK);

    freq=(long)rtlsdr_get_center_freq(dev);
    if (freq==0)
        return last_freq;
    last_freq=freq;
    return freq;
}

//long LIBRTL_API __stdcall GetHWSR()
extern "C"
long GetHWSR()
{
    //MessageBox(NULL, TEXT("GetHWSR"),NULL, MB_OK);
    return (long)rtlsdr_get_sample_rate(dev);
}
//int LIBRTL_API __stdcall ExtIoGetSrates( int srate_idx, double * samplerate )
extern "C"
int ExtIoGetSrates( int srate_idx, double * samplerate )
{
    //MessageBox(NULL, TEXT("ExtIoGetSrates"),NULL, MB_OK);

    if ( srate_idx < (sizeof(samplerates)/sizeof(samplerates[0])) )
    {
        *samplerate=  samplerates[srate_idx].value;
        return 0;
    }
    return 1;	// ERROR
}

//int  LIBRTL_API __stdcall ExtIoGetActualSrateIdx(void)
extern "C"
int  ExtIoGetActualSrateIdx(void)
{
    int ret=0;
    //Conversion from String to int
    sscanf(reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->SampleRateCombo->currentText().toLocal8Bit().data(), "%d", &ret);

    return ret;
}
//int  LIBRTL_API __stdcall ExtIoSetSrate( int srate_idx )
extern "C"
int ExtIoSetSrate( int srate_idx )
{
    //	MessageBox(NULL, str, NULL, MB_OK);

    if (  srate_idx>=0 && srate_idx < (sizeof(samplerates)/sizeof(samplerates[0])) )
    {
    //		MessageBox(NULL, TEXT("ExtIoSetSrate"),NULL, MB_OK);
        rtlsdr_set_sample_rate(dev, samplerates[srate_idx].value);
        //TODO ITS A COMBOBOX NOT A TEXT INPUT ALSO SETTING A CONSTANT SIZE MIGHT NOT BE A GOOD IDEA
        char str[32];
        snprintf(str, 32, "%d", srate_idx);
        reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->SampleRateCombo->setCurrentText(str);
        //ComboBox_SetCurSel(GetDlgItem(h_dialog,IDC_SAMPLERATE),srate_idx);


        //WinradCallBack(-1,WINRAD_SRCHANGE,0,NULL);// Signal application
        return 0;
    }
    return 1;	// ERROR
}
//int  LIBRTL_API __stdcall GetAttenuators( int atten_idx, float * attenuation )
extern "C"
int GetAttenuators( int atten_idx, float * attenuation )
{
//	MessageBox(NULL, TEXT("GetAttenuators"),NULL, MB_OK);

    if ( atten_idx < n_gains )
    {
        *attenuation= gains[atten_idx]/10.0;
        return 0;
    }
    return 1;
}
//int  LIBRTL_API __stdcall GetActualAttIdx(void)
extern "C"
int GetActualAttIdx(void)
{
//	MessageBox(NULL, TEXT("GetActualAttIdx"),NULL, MB_OK);
    for (int i=0;i<n_gains;i++)
        if (last_gain==gains[i])
            return i;
    return -1;
}

//int  LIBRTL_API __stdcall SetAttenuator( int atten_idx )
extern "C"
int SetAttenuator( int atten_idx ){
//	MessageBox(NULL, TEXT("SetAttenuator"),NULL, MB_OK);

    if ( atten_idx<0 || atten_idx > n_gains )
        return -1;

    int pos=gains[atten_idx];

    //SendMessage(GetDlgItem(h_dialog,IDC_GAIN),  TBM_SETPOS  , (WPARAM)TRUE, (LPARAM)-pos);
    //TODO WHY IS EVERY VALUE NEGATIVE IN THE ORIGINAL CODE
    reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->TunerGainSlider->setValue(-pos);

    //TOCHECK THIS WONT WORK!?, also the original had some kind of label _stprintf_s(str,255, TEXT("%2.1f  dBm"),(float) pos/10);
    if(reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->TunerAGCCheck->checkState() == Qt::CheckState::Unchecked ){
        //char str[255];
        if (pos!=last_gain){
            rtlsdr_set_tuner_gain(dev,pos);
        }
    }
    last_gain=pos;
    return 0;
}
//int   LIBRTL_API __stdcall ExtIoGetAGCs( int agc_idx, char * text )
extern "C"
int ExtIoGetAGCs( int agc_idx, char * text ){

    //MessageBox(NULL, TEXT("ExtIoGetAGCs"),NULL, MB_OK);
    switch ( agc_idx )
    {
        case 0:	snprintf( text, 16, "%s", "None" );
                return 0;
        case 1:	snprintf( text, 16, "%s", "Tuner AGC" );
                return 0;
        case 2:	snprintf( text, 16, "%s", "RTL AGC" );
                return 0;
        case 3:	snprintf( text, 16, "%s", "RTL+Tuner AGC" );
                return 0;
        default:	return -1;
    }
    return -1;
}


//int   LIBRTL_API __stdcall ExtIoGetActualAGCidx (void)
extern "C"
int  ExtIoGetActualAGCidx (void)
{
    //MessageBox(NULL, TEXT("ExtIoGetActualAGCidx"),NULL, MB_OK);
    return HDSDR_AGC;
}

//TODO on the last line theres an if that suggests that agc can't be 0 read more about it
//int   LIBRTL_API __stdcall ExtIoSetAGC (int agc_idx)
extern "C"
int ExtIoSetAGC (int agc_idx){
    //MessageBox(NULL, TEXT("ExtIoSetAGC"),NULL, MB_OK);
    //char str[255];
    //_stprintf_s(str,255, TEXT("O valor � %d, era %d"), agc_idx, HDSDR_AGC);
    //MessageBox(NULL, str, NULL, MB_OK);
    HDSDR_AGC = agc_idx;
    //if (HDSDR_AGC==0) HDSDR_AGC=3;
    return 0;
}
//TOOPTIMIZE currentData(); appears to be a function that returns "whatever" type you feed it
//TOOPTIMIZE theres a lot of things that are shared this is an overall cluterred and bloated function
//int   LIBRTL_API __stdcall ExtIoGetSetting( int idx, char * description, char * value )
extern "C"
int ExtIoGetSetting( int idx, char * description, char * value ){
    //MessageBox(NULL, TEXT("ExtIoGetSetting"),NULL, MB_OK);
    switch ( idx ){
        case 0:	{
                snprintf( description, 1024, "%s", "SampleRateIdx" );
                snprintf( value, 1024, "%d", ExtIoGetActualSrateIdx());
                return 0;
        }
        case 1:	{
                snprintf( description, 1024, "%s", "Tuner_AGC" );
                snprintf( value, 1024, "%d", reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->TunerAGCCheck->checkState() == Qt::CheckState::Checked? 1 : 0 );
                return 0;
        }
        case 2:	{
                snprintf( description, 1024, "%s", "RTL_AGC" );
                snprintf( value, 1024, "%d", reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->RTLAGCCheck->checkState() == Qt::CheckState::Checked ?1:0 );
                return 0;
        }
        case 3:	{
                snprintf( description, 1024, "%s", "Frequency_Correction" );
                //Edit_GetText(GetDlgItem(h_dialog,IDC_PPM), ppm, 255 );
                snprintf( value, 1024, "%d", reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->FrequencyCorrectionSpin->value() );
                return 0;
        }
        case 4:	{
                snprintf( description, 1024, "%s", "Tuner_Gain" );
                //int pos=-SendMessage(GetDlgItem(h_dialog,IDC_GAIN),  TBM_GETPOS  , (WPARAM)0, (LPARAM)0);
                int pos=-reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->TunerGainSlider->value();
                snprintf( value, 1024, "%d",  pos );
                return 0;
        }
        case 5:	{
                snprintf( description, 1024, "%s", "Buffer_Size" );
                int ret=0;
                //Conversion from String to int
                sscanf(reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->BufferSizeCombo->currentText().toLocal8Bit().data(), "%d", &ret);
                snprintf( value, 1024, "%d", ret);
                return 0;
        }
        case 6:	{
                snprintf( description, 1024, "%s", "Offset_Tuning" );
                snprintf( value, 1024, "%d", reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->OffsetTuningCheck->checkState() == Qt::CheckState::Checked ?1:0);
                return 0;
        }
        case 7:	{
                snprintf( description, 1024, "%s", "Direct_Sampling" );
                int ret=0;
                //Conversion from String to int
                sscanf(reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->BufferSizeCombo->currentText().toLocal8Bit().data(), "%d", &ret);
                snprintf( value, 1024, "%d",  ret);
                return 0;
        }
        case 8:	{
            snprintf( description, 1024, "%s", "Device" );
            int ret=0;
            //Conversion from String to int
            sscanf(reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->BufferSizeCombo->currentText().toLocal8Bit().data(), "%d", &ret);
            snprintf( value, 1024, "%d",  ret);
            return 0;
        }
        //RANT HOL... PiCk a indentation style its barely readable
        default:
            return -1;
    }
    return -1;
}

//void  LIBRTL_API __stdcall ExtIoSetSetting( int idx, const char * value )
extern "C"
void ExtIoSetSetting( int idx, const char * value ){
    //MessageBox(NULL, TEXT(" ExtIoSetSetting"),NULL, MB_OK);
    int tempInt;

    switch ( idx )
    {
    case 0:
                tempInt = atoi( value );
                if (  tempInt>=0 && tempInt < (sizeof(samplerates)/sizeof(samplerates[0])) )
                {
                    samplerate_default=tempInt;
                }
                break;
    case 1:		tempInt = atoi( value );
                TunerAGC_default=tempInt?1:0;
                break;
    case 2:		tempInt = atoi( value );
                RTLAGC_default=tempInt?1:0;
                break;
    case 3:		tempInt = atoi( value );
                if (  tempInt>MIN_PPM && tempInt < MAX_PPM )
                {
                    ppm_default=tempInt;
                }
                break;
    case 4:		tempInt = atoi( value );
                gain_default=tempInt;
                break;
    case 5:		tempInt = atoi( value );
                if (  tempInt>=0 && tempInt < (sizeof(buffer_sizes)/sizeof(buffer_sizes[0])) )
                {
                buffer_default=tempInt;
                }
                break;
    case 6:		tempInt = atoi( value );
                OffsetT_default=tempInt?1:0;
                break;
    case 7:		tempInt = atoi( value );
                directS_default=tempInt;
                break;
    case 8:		tempInt = atoi( value );
                device_default=tempInt;
                break;

    }
}
//TODO kinda weird enabling devices and direct combo's at this phase but might have to do with something around windows32, check if this is really needed
//void LIBRTL_API __stdcall StopHW()
extern "C"
void StopHW(){
    //MessageBox(NULL, TEXT("StopHW"),NULL, MB_OK);
    //Stop_Thread();
    //TODO Wasnt this done somewhere upstairs? if so it should be delete[] blah
    delete short_buf;
    //uhm? what?
    reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->DeviceComboBox->setEnabled(true);
    reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->HFDirectSamplingCombo->setEnabled(true);
    //EnableWindow(GetDlgItem(h_dialog,IDC_DEVICE),TRUE);
    //EnableWindow(GetDlgItem(h_dialog,IDC_DIRECT),TRUE);
}

//void LIBRTL_API __stdcall CloseHW()
extern "C"
void CloseHW(){
    //MessageBox(NULL, TEXT("CloseHW"),NULL, MB_OK);
    rtlsdr_close(dev);
    dev=NULL;
    if (reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)!=NULL){
        //TODO this might not be the best idea
        reinterpret_cast<Ui_TSDRPlugin_ExtIO *>(handle)->~Ui_TSDRPlugin_ExtIO();
    }
}
//void LIBRTL_API __stdcall ShowGUI()
//TODO
extern "C"
void ShowGUI(){

    //ShowWindow(h_dialog,SW_SHOW);
    //SetForegroundWindow(h_dialog);
    return;
}
//void LIBRTL_API  __stdcall HideGUI()
//TODO
extern "C"
void HideGUI(){
    //ShowWindow(h_dialog,SW_HIDE);
    return;
}
//void LIBRTL_API  __stdcall SwitchGUI()
//TODO
extern "C"
void SwitchGUI()
{
    /*
    if (IsWindowVisible(h_dialog))
        ShowWindow(h_dialog,SW_HIDE);
    else
        ShowWindow(h_dialog,SW_SHOW);
    */
    return;
}

//void LIBRTL_API __stdcall SetCallback(void (* myCallBack)(int, int, float, void *))
extern "C"
void SetCallback(void (* pfnExtIOCallback)(int cnt, int status, float IQoffs, void *IQdata)){

    WinradCallBack = pfnExtIOCallback;
    return;
}

void RTLSDRCallBack(unsigned char *buf, uint32_t len, void *ctx)
{
    if(len == buffer_len)
    {
        short *short_ptr = (short*)&short_buf[0];
        unsigned char* char_ptr = buf;

        for(uint32_t i = 0 ; i < len;i++)
        {
            (*short_ptr) = ((short)(*char_ptr)) - 128;
            char_ptr++;
            short_ptr++;
        }
        WinradCallBack(buffer_len,0,0,(void*)short_buf);
    }
}

int TSDRPlugin_ExtIO::Start_Thread(){
    //TODO CHECK:
    //If already running, exit


    //if(worker_handle != INVALID_HANDLE_VALUE)
    //	return -1;

    //reset endpoin
    if(rtlsdr_reset_buffer(dev) < 0)
        return -1;
    std::thread worker(ThreadProc, nullptr);
    worker.join();

    //worker_handle = (HANDLE) _beginthread( ThreadProc, 0, NULL );
    //if(worker_handle == INVALID_HANDLE_VALUE)
    //	return -1;

    return 0;
}
//TODO MULTITHREADING
int TSDRPlugin_ExtIO::Stop_Thread(){
    //if(worker_handle == INVALID_HANDLE_VALUE)
    //	return -1;

    rtlsdr_cancel_async(dev);
    // Wait 1s for thread to die
    //WaitForSingleObject(worker_handle,INFINITE);
    //CloseHandle(worker_handle);
    //worker_handle=INVALID_HANDLE_VALUE;
    return 0;
}
//TODO MULTITHREADING
void ThreadProc(void *p)
{


    /* Blocks until rtlsdr_cancel_async() is called */
    /* Use default number of buffers */

    rtlsdr_read_async(dev, (rtlsdr_read_async_cb_t)&RTLSDRCallBack, NULL, 0, buffer_len);

    //_endthread();

}

// EL MONSTRUOSIDAD ABERRANTE DEVORADOR DE HOMBRES DE TRES CABEZAS
//TOOPTIMIZE holy... this function just sucks like its not even setting defaults the original MainDlgProc just seems to be dread inducing
int* MainCallback(){
    //TOCHECK TOOPTIMIZE is this really necessary? Like seems too spagheti for a driver
    for (int i=0; i<(sizeof(directS)/sizeof(directS[0]));i++){
        //ComboBox_AddString(GetDlgItem(hwndDlg,IDC_DIRECT),directS[i]);
        handle->HFDirectSamplingCombo->addItem(directS[i]);
    }
    handle->HFDirectSamplingCombo->setCurrentIndex(directS_default);
    rtlsdr_set_direct_sampling(dev, directS_default);
    //TOCHECK wtf, so its a default that acts like a variable? or is it dependant on the model of the hardware?
    handle->TunerAGCCheck->setCheckState(TunerAGC_default? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    //TOCHECKURGENT why is 0 the true one?
    rtlsdr_set_tuner_gain_mode(dev,TunerAGC_default?0:1);

    //TOCHECK wtf, so its a default that acts like a variable? or is it dependant on the model of the hardware?
    handle->RTLAGCCheck->setCheckState(RTLAGC_default? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    //TOCHECKURGENT see tocheckurgent above, this time its inverted either people doing the driver have some delusional mindset or theres some weird convention mindset, or this dll SUCK'S!
    rtlsdr_set_agc_mode(dev,RTLAGC_default?1:0);

    handle->OffsetTuningCheck->setCheckState(OffsetT_default? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    //here again theres this weird abomination;
    rtlsdr_set_offset_tuning(dev,OffsetT_default?1:0);

    //RANT bellow this comment line you can find a masterpiece of windows engineering you might be wondering what does it do, but basically its defining the maximum and minimum for the frequency correction spinner, and not the whole component aparently its just the arrow controls
    //SendMessage(GetDlgItem(hwndDlg,IDC_PPM_S), UDM_SETRANGE  , (WPARAM)TRUE, (LPARAM)MAX_PPM | (MIN_PPM << 16));
    //TOCHECK as stated previously this is absolutely disgusting i don't even think you can do this, either way might be some weird convention
    handle->FrequencyCorrectionSpin->setMaximum(MAX_PPM);
    handle->FrequencyCorrectionSpin->setMinimum(MIN_PPM << 16);

    //RANT ANOTHER MeMe the original should return a pointer to an int and instead return a false(bool) i guess its supposed to return a blank pointer?
    //return false;
    return 0;

}

//static INT_PTR CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
/*static MainDlgProc(uint32_t uMsg, char * wParam, long lParam){

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            TCHAR tempStr[255];
            _stprintf_s(tempStr,255, TEXT("%d"), ppm_default);
            Edit_SetText(GetDlgItem(hwndDlg,IDC_PPM), tempStr );
            rtlsdr_set_freq_correction(dev, ppm_default);

            for (int i=0; i<device_count;i++)
            {
                TCHAR str[255];
                _stprintf_s(str,255,  TEXT("(%d) - %S %S %S"),i+1, connected_devices[i].product,connected_devices[i].vendor,connected_devices[i].serial);
                ComboBox_AddString(GetDlgItem(hwndDlg,IDC_DEVICE),str);
            }
            ComboBox_SetCurSel(GetDlgItem(hwndDlg,IDC_DEVICE),device_default);

            for (int i=0; i<(sizeof(samplerates)/sizeof(samplerates[0]));i++)
            {
                ComboBox_AddString(GetDlgItem(hwndDlg,IDC_SAMPLERATE),samplerates[i].name);
//				MessageBox(NULL,sample_rates[i],TEXT("Mensagem"),MB_OK);
            }
            ComboBox_SetCurSel(GetDlgItem(hwndDlg,IDC_SAMPLERATE),samplerate_default);

            for (int i=0; i<(sizeof(buffer_sizes)/sizeof(buffer_sizes[0]));i++)
            {
                TCHAR str[255];
                _stprintf_s(str,255, TEXT("%d kB"),buffer_sizes[i]);
                ComboBox_AddString(GetDlgItem(hwndDlg,IDC_BUFFER),str);
            }
            ComboBox_SetCurSel(GetDlgItem(hwndDlg,IDC_BUFFER),buffer_default);
            buffer_len=buffer_sizes[buffer_default]*1024;



            n_gains = rtlsdr_get_tuner_gains(dev,NULL);
            gains = new int[n_gains];
            hGain = GetDlgItem(hwndDlg,IDC_GAIN);

            rtlsdr_get_tuner_gains(dev,gains);
            SendMessage(hGain, TBM_SETRANGEMIN , (WPARAM)TRUE, (LPARAM)-gains[n_gains-1]);
            SendMessage(hGain, TBM_SETRANGEMAX , (WPARAM)TRUE, (LPARAM)-gains[0]);
            int gain_d_index=0;
            for(int i=0; i<n_gains;i++)
            {
                SendMessage(hGain, TBM_SETTIC , (WPARAM)0, (LPARAM)-gains[i]);
                if (gain_default==gains[i]) gain_d_index=i;
            }
            SendMessage(hGain,  TBM_SETPOS  , (WPARAM)TRUE, (LPARAM)-gains[gain_d_index]);

            if (TunerAGC_default) {
                EnableWindow(hGain,FALSE);
                Static_SetText(GetDlgItem(hwndDlg,IDC_GAINVALUE),TEXT("AGC"));
            } else {
                int pos=-SendMessage(hGain,  TBM_GETPOS  , (WPARAM)0, (LPARAM)0);
                TCHAR str[255];
                _stprintf_s(str,255, TEXT("%2.1f dB"),(float) pos/10);
                Static_SetText(GetDlgItem(hwndDlg,IDC_GAINVALUE),str);
                rtlsdr_set_tuner_gain(dev,gains[gain_d_index]);
            }
            last_gain=gains[gain_d_index];

            return TRUE;
        }
        case WM_COMMAND:
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
                case IDC_PPM:
                    if(GET_WM_COMMAND_CMD(wParam, lParam) == EN_CHANGE)
                    {
                        TCHAR ppm[255];
                        Edit_GetText((HWND) lParam, ppm, 255 );
                        if (!rtlsdr_set_freq_correction(dev, _ttoi(ppm)))
                            WinradCallBack(-1,WINRAD_LOCHANGE,0,NULL);
                    //	else
                    //	{
                    //		TCHAR str[255];
                    //		_stprintf_s(str,255, TEXT("O valor � %d"), _ttoi(ppm));
                    //		MessageBox(NULL, str, NULL, MB_OK);
                    //	}
                    }
                    return TRUE;
                case IDC_RTLAGC:
                {
                    if(Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) //it is checked
                    {
                        rtlsdr_set_agc_mode(dev,1);
//						MessageBox(NULL,TEXT("It is checked"),TEXT("Message"),0);
                    }
                    else //it has been unchecked
                    {
                        rtlsdr_set_agc_mode(dev,0);
//						MessageBox(NULL,TEXT("It is unchecked"),TEXT("Message"),0);
                    }
                    return TRUE;
                }
                case IDC_OFFSET:
                {
                    if(Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) //it is checked
                    {
                        rtlsdr_set_offset_tuning(dev,1);
//						MessageBox(NULL,TEXT("It is checked"),TEXT("Message"),0);
                    }
                    else //it has been unchecked
                    {
                        rtlsdr_set_offset_tuning(dev,0);
//						MessageBox(NULL,TEXT("It is unchecked"),TEXT("Message"),0);
                    }
                    return TRUE;
                }
                case IDC_TUNERAGC:
                {
                    if(Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) //it is checked
                    {
                        rtlsdr_set_tuner_gain_mode(dev,0);

                        EnableWindow(hGain,FALSE);
                        Static_SetText(GetDlgItem(hwndDlg,IDC_GAINVALUE),TEXT("AGC"));
//						MessageBox(NULL,TEXT("It is checked"),TEXT("Message"),0);
                    }
                    else //it has been unchecked
                    {
                        rtlsdr_set_tuner_gain_mode(dev,1);

                        EnableWindow(hGain,TRUE);

                        int pos=-SendMessage(hGain,  TBM_GETPOS  , (WPARAM)0, (LPARAM)0);
                        TCHAR str[255];
                        _stprintf_s(str,255, TEXT("%2.1f dB"),(float) pos/10);
                        Static_SetText(GetDlgItem(hwndDlg,IDC_GAINVALUE),str);

                        rtlsdr_set_tuner_gain(dev,pos);
//						MessageBox(NULL,TEXT("It is unchecked"),TEXT("Message"),0);
                    }
                    return TRUE;
                }
                case IDC_SAMPLERATE:
                    if(GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                    {
                        rtlsdr_set_sample_rate(dev, samplerates[ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam))].value);
                        WinradCallBack(-1,WINRAD_SRCHANGE,0,NULL);// Signal application

                        //TCHAR  ListItem[256];
                        //ComboBox_GetLBText((HWND) lParam,ComboBox_GetCurSel((HWND) lParam),ListItem);
                        //MessageBox(NULL, ListItem, TEXT("Item Selected"), MB_OK);
                        //TCHAR str[255];
                        //_stprintf(str, TEXT("O valor � %d"), samplerates[ComboBox_GetCurSel((HWND) lParam)].value);
                        //MessageBox(NULL, str, NULL, MB_OK);
                    }
                    if(GET_WM_COMMAND_CMD(wParam, lParam) ==  CBN_EDITUPDATE)
                    {
                        TCHAR  ListItem[256];

                        ComboBox_GetText((HWND) lParam,ListItem,256);
                        //MessageBox(NULL, ListItem, TEXT("Item Selected"), MB_OK);

                        TCHAR *endptr;
                        double coeff = _tcstod(ListItem, &endptr);

                        while (_istspace(*endptr)) ++endptr;

                        int exp = 1;
                        switch (_totupper(*endptr)) {
                            case 'K': exp = 1024; break;
                            case 'M': exp = 1024*1024; break;
                        }

                        uint32_t newrate=coeff*exp;
                        if (newrate>=MINRATE && newrate<=MAXRATE) {
                            rtlsdr_set_sample_rate(dev, newrate);
                            WinradCallBack(-1,WINRAD_SRCHANGE,0,NULL);// Signal application
                        }

                        //TCHAR str[255];
                        //_stprintf(str, TEXT("O valor � %d"), newrate);
                        //MessageBox(NULL, str, NULL, MB_OK);
                    }
                    //MessageBox(NULL,TEXT("Bitrate"),TEXT("Mensagem"),MB_OK);
                    return TRUE;
                case IDC_BUFFER:
                    if(GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                    {
                        buffer_len=buffer_sizes[ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam))]*1024;
                        WinradCallBack(-1,WINRAD_SRCHANGE,0,NULL);// Signal application
                    }
                    return TRUE;
                case IDC_DIRECT:
                    if(GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                    {
                        rtlsdr_set_direct_sampling(dev, ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam)));
                        if (ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam)) == 0)
                            if(Button_GetCheck(GetDlgItem(hwndDlg,IDC_OFFSET)) == BST_CHECKED)
                                rtlsdr_set_offset_tuning(dev,1);
                            else
                                rtlsdr_set_offset_tuning(dev,0);
                        WinradCallBack(-1,WINRAD_LOCHANGE,0,NULL);// Signal application
                    }
                    return TRUE;
                case IDC_DEVICE:
                    if(GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                    {
                        uint32_t tempSrate = rtlsdr_get_sample_rate(dev);
                        rtlsdr_close(dev);
                        dev=NULL;
                        if(rtlsdr_open(&dev,ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam))) < 0)
                        {
                            MessageBox(NULL,TEXT("Cound't open device!"),
                                        TEXT("ExtIO RTL"),
                                        MB_ICONERROR | MB_OK);
                            return TRUE;
                        }
                        rtlsdr_set_sample_rate(dev, tempSrate);
                    }
                    return TRUE;

            }
            break;
        case WM_VSCROLL:
            //if (LOWORD(wParam)!=TB_THUMBTRACK && LOWORD(wParam)!=TB_ENDTRACK)

            if ((HWND)lParam==hGain)
            {

                int pos = -SendMessage(hGain,  TBM_GETPOS  , (WPARAM)0, (LPARAM)0);
                for (int i=0;i<n_gains-1;i++)
                    if (pos>gains[i] && pos<gains[i+1])
                        if((pos-gains[i])<(gains[i+1]-pos) && (LOWORD(wParam)!=TB_LINEUP) || (LOWORD(wParam)==TB_LINEDOWN))
                            pos=gains[i];
                        else
                            pos=gains[i+1];

                SendMessage(hGain,  TBM_SETPOS  , (WPARAM)TRUE, (LPARAM)-pos);
                TCHAR str[255];
                _stprintf_s(str,255, TEXT("%2.1f  dBm"),(float) pos/10);
                Static_SetText(GetDlgItem(hwndDlg,IDC_GAINVALUE),str);

                if (pos!=last_gain)
                {
                    last_gain=pos;
                    rtlsdr_set_tuner_gain(dev,pos);
                    WinradCallBack(-1,WINRAD_ATTCHANGE,0,NULL);
                }

                // MessageBox(NULL, str, NULL, MB_OK);
                return TRUE;
            }
            if ((HWND)lParam==GetDlgItem(hwndDlg,IDC_PPM_S))
            {
    //			MessageBox(NULL,TEXT("ola"),NULL, MB_OK);
                return TRUE;
            }
            //	TCHAR str[255];
            //	_stprintf(str, TEXT("%d"),LOWORD(wParam));
            //	MessageBox(NULL, str, NULL, MB_OK);
            break;


        //case WM_SYSCOMMAND:
        //	switch (wParam & 0xFFF0)
  //          {
        //		case SC_SIZE:
        //		case SC_MINIMIZE:
        //		case SC_MAXIMIZE:
  //                  return TRUE;
  //          }
  //          break;
        case WM_CLOSE:
            ShowWindow(h_dialog,SW_HIDE);
            return TRUE;
            break;
        case WM_DESTROY:
            delete[] gains;
            h_dialog=NULL;
            return TRUE;
            break;


        case WM_CTLCOLORSTATIC:
            if ( IDC_PLL == GetDlgCtrlID((HWND)lParam))
            {
                HDC hdc = (HDC)wParam;
                if (pll_locked==0)
                {
                    SetBkColor(hdc, RGB(0,255,0));
                    return (INT_PTR)BRUSH_GREEN;
                } else {
                    SetBkColor(hdc, RGB(255,0,0));
                    return (INT_PTR)BRUSH_RED;
                }
            }
            break;


    }

    return false;
}
*/


