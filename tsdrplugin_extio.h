#ifndef TSDRPLUGIN_EXTIO_H
#define TSDRPLUGIN_EXTIO_H
#ifdef WIN43
	#define LIBRTL_API __declspec(dllexport)
#else
	#define LIBRTL_API
#endif

#include <QMainWindow>
#include <string>
#include<stdio.h>
#include<stdlib.h>

QT_BEGIN_NAMESPACE
namespace Ui { class TSDRPlugin_ExtIO; }
QT_END_NAMESPACE

#ifdef __cplusplus
extern "C"
{
#endif

struct Ui_TSDRPlugin_ExtIO; // An opaque type that we'll use as a handle
typedef struct Ui_TSDRPlugin_ExtIO Ui_TSDRPlugin_ExtIO;
struct QMessageBox;
typedef struct QMessageBox QMessageBox;
#ifdef __cplusplus
}
#endif


class TSDRPlugin_ExtIO : public QMainWindow
{
    Q_OBJECT

public:
    TSDRPlugin_ExtIO(QWidget *parent = nullptr);
    ~TSDRPlugin_ExtIO();
    int Start_Thread();
    int Stop_Thread();
    //void  ThreadProc(void *p);

private:
    Ui::TSDRPlugin_ExtIO *ui;
};
#endif // TSDRPLUGIN_EXTIO_H
