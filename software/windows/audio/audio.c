#include <stdio.h>  
#include <windows.h>  
#pragma comment(lib, "winmm.lib")
//
#define bufsize 1024*1024	//ÿ�ο���10k�Ļ���洢¼������
BYTE pBuffer1[bufsize];		//�ɼ���Ƶʱ�����ݻ���
FILE* pcmfile;  //��Ƶ�ļ�  
//
HWAVEIN hWaveIn;			//�����豸
HWAVEOUT hwo;
//
WAVEFORMATEX fmt;		//�ɼ���Ƶ�ĸ�ʽ���ṹ��
WAVEHDR head;				//�ɼ���Ƶʱ�������ݻ���Ľṹ��




int record()
{
    HANDLE          wait;
    fmt.wFormatTag = WAVE_FORMAT_PCM;//������ʽΪPCM
    fmt.nSamplesPerSec = 48000;//�����ʣ�16000��/��
    fmt.wBitsPerSample = 16;//�������أ�16bits/��
    fmt.nChannels = 1;//������������2����
    fmt.nAvgBytesPerSec = 16000;//ÿ��������ʣ�����ÿ���ܲɼ������ֽڵ�����
    fmt.nBlockAlign = 2;//һ����Ĵ�С������bit���ֽ�������������
    fmt.cbSize = 0;//һ��Ϊ0
 
    wait = CreateEvent(NULL, 0, 0, NULL);
    //ʹ��waveInOpen����������Ƶ�ɼ�
    waveInOpen(&hWaveIn, WAVE_MAPPER, &fmt,(DWORD_PTR)wait, 0L, CALLBACK_EVENT);
 
    //�����������飨������Խ���������飩����������Ƶ����
    int i = 2;
    fopen_s(&pcmfile, "test.pcm", "wb");
    while (i--)//¼��20�����������������Ƶ��������紫������޸�Ϊʵʱ¼�����ŵĻ�����ʵ�ֶԽ�����
    {
        head.lpData = (LPSTR)pBuffer1;
        head.dwBufferLength = bufsize;
        head.dwBytesRecorded = 0;
        head.dwUser = 0;
        head.dwFlags = 0;
        head.dwLoops = 1;
        waveInPrepareHeader(hWaveIn, &head, sizeof(WAVEHDR));//׼��һ���������ݿ�ͷ����¼��
        waveInAddBuffer(hWaveIn, &head, sizeof (WAVEHDR));//ָ���������ݿ�Ϊ¼�����뻺��
        waveInStart(hWaveIn);//��ʼ¼��
        Sleep(1000);//�ȴ�����¼��1s
        waveInReset(hWaveIn);//ֹͣ¼��
        fwrite(pBuffer1, 1, head.dwBytesRecorded, pcmfile);
        printf("%ds\n", i);
    }
    fclose(pcmfile);
 
    waveInClose(hWaveIn);
    return 0;
}




static void CALLBACK WaveCallback(HWAVEOUT hWave, UINT uMsg, DWORD dwInstance, DWORD dw1, DWORD dw2)//�ص�����  
{  
    switch (uMsg)  
    {  
        case WOM_DONE://�ϴλ��沥�����,�������¼�  
        {
			head.dwBufferLength = 0;
            printf("done\n");
        }  
    }
}
void play(char* name)   
{
	int ret;
    fmt.wFormatTag = WAVE_FORMAT_PCM;//���ò��������ĸ�ʽ  
    fmt.nChannels = 1;//������Ƶ�ļ���ͨ������  
    fmt.nSamplesPerSec = 48000;//����ÿ���������źͼ�¼ʱ������Ƶ��  
    fmt.nAvgBytesPerSec = 16000;//���������ƽ�����ݴ�����,��λbyte/s�����ֵ���ڴ��������С�Ǻ����õ�  
    fmt.nBlockAlign = 2;//���ֽ�Ϊ��λ���ÿ����  
    fmt.wBitsPerSample = 16;  
    fmt.cbSize = 0;//������Ϣ�Ĵ�С

    fopen_s(&pcmfile, name, "rb");//���ļ�  
    waveOutOpen(&hwo, WAVE_MAPPER, &fmt, (DWORD)WaveCallback, 0L, CALLBACK_FUNCTION);//��һ�������Ĳ�����Ƶ���װ���������������ţ���ʽΪ�ص�������ʽ������ǶԻ�����򣬿��Խ������������Ϊ(DWORD)this����������Demo��������  

    ret = fread(pBuffer1, 1, bufsize, pcmfile);
    head.dwLoops = 0L;
    head.lpData = pBuffer1;
    head.dwBufferLength = ret;   
    head.dwFlags = 0L;
    waveOutPrepareHeader(hwo, &head, sizeof(WAVEHDR));//׼��һ���������ݿ����ڲ���  
    waveOutWrite(hwo, &head, sizeof(WAVEHDR));//����Ƶý���в��ŵڶ�������ָ�������ݣ�Ҳ�൱�ڿ���һ������������˼  

    while (head.dwBufferLength != 0)//����ļ�����û��������ȴ�500ms  
    {  
        Sleep(500);  
    }
    waveOutUnprepareHeader(hwo, &head, sizeof(WAVEHDR)); 

    fclose(pcmfile);//�ر��ļ�  
    return;  
}




void main(int argc, char** argv)
{
	if(argc == 1)record();
	else play("test.pcm");
}