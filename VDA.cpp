#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdint.h>
#include <cmath>
#include <bitset>
#include <vector>
#include <sstream>
using namespace std;


struct WavData {
public:
	int16_t *data;
	int32_t pointSize; //���ݵ�ĸ���
	
	int32_t size;
	int16_t format_tag, channels, block_align, bits_per_sample;
	int32_t format_length, sample_rate, avg_bytes_sec, data_size;
	
	WavData() {
		format_tag = -1, channels = -1, block_align = -1, bits_per_sample = -1;
		format_length = 1, sample_rate = -1, avg_bytes_sec = -1, data_size = -1;
		data = NULL;
		pointSize = 0;
	}
};

bool sgn(int16_t val);
double caculateZeroCroRates(WavData& speechAudio, int frameBegin, int frameEnd);
double caculateMeanEnergy(WavData& speechAudio, int frameBegin, int frameEnd);
int processSpeech(WavData& speechAudio);
void freeSource(WavData* data);
void loadWavFile(const char* fname, WavData *ret);
vector<pair<int, int> > cutFrame(vector<double> & meanEnergys, vector<double> & zeroCroRates);
vector<pair<int, int > >  cutWavBySil(vector<pair<int, int > > silencePair, WavData& speechAudio);
void genWav(vector<pair<int, int > > speechPair, WavData& speechAudio);



void writeWavFile(const char * fname, WavData * spFile){
	FILE* fp = fopen(fname, "ab+");
	if(fp){  
		char id[5];
		int32_t size = spFile -> size;
		int16_t format_tag, channels, block_align, bits_per_sample;
		int32_t format_length, sample_rate, avg_bytes_sec, data_size;
		
		id[0] = 'R';id[1] = 'I';id[2] = 'F';id[3] = 'F';
		fwrite("RIFF",sizeof(char), 4, fp);
		fwrite(&spFile->size, sizeof(int32_t), 1, fp);
		fwrite("WAVE",sizeof(char), 4, fp);
		fwrite("fmt ",sizeof(char), 4, fp);
		fwrite(&spFile->format_length, sizeof(int16_t), 2, fp);
		fwrite(&spFile->format_tag, sizeof(int16_t), 1, fp);
		fwrite(&spFile->channels, sizeof(int16_t), 1, fp);
		fwrite(&spFile->sample_rate, sizeof(int16_t), 2, fp);
		fwrite(&spFile->avg_bytes_sec, sizeof(int16_t), 2, fp);
		fwrite(&spFile->block_align, sizeof(int16_t), 1, fp);
		fwrite(&spFile->bits_per_sample, sizeof(int16_t), 1, fp);
		fwrite("data", sizeof(char), 4, fp);
		fwrite(&spFile->data_size, sizeof(int16_t), 2, fp);
		//д��data����
		for(int i = 0; i < spFile->pointSize; i++){
			unsigned char head = spFile->data[i] & 0xff00 >>8;
			unsigned char tail = spFile->data[i] & 0x00ff;
			fwrite(&tail, sizeof(unsigned char), 1, fp);
			fwrite(&head, sizeof(unsigned char), 1, fp); 
		}
	} 
	fclose(fp);
} 



void loadWavFile(const char* fname, WavData *spFile) {
	FILE* fp = fopen(fname, "rb");
	if (fp) {
		char id[5];
		int32_t size;
		int16_t format_tag, channels, block_align, bits_per_sample;
		int32_t format_length, sample_rate, avg_bytes_sec, data_size;

		fread(id, sizeof(char), 4, fp);
		id[4] = '\0';
		cout << "id "<<id <<endl;
		
		if (!strcmp(id, "RIFF")) {
			fread(&size, sizeof(int16_t), 2, fp);
			fread(id, sizeof(char), 4, fp);
			id[4] = '\0';
			if (!strcmp(id, "WAVE")) {
				fread(id, sizeof(char), 4, fp); // "fmt"
				fread(&format_length, sizeof(int16_t), 2, fp);
				fread(&format_tag, sizeof(int16_t), 1, fp);
				fread(&channels, sizeof(int16_t), 1, fp);
				fread(&sample_rate, sizeof(int16_t), 2, fp);
				fread(&avg_bytes_sec, sizeof(int16_t), 2, fp);
				fread(&block_align, sizeof(int16_t), 1, fp);
				fread(&bits_per_sample, sizeof(int16_t), 1, fp);	
				fread(id, sizeof(char), 4, fp);
				fread(&data_size, sizeof(int16_t), 2, fp);
				spFile->pointSize = data_size / (bits_per_sample / 8);  //�ж��ٸ�����
				// ��̬�����˿ռ䣬�ǵ�Ҫ�ͷ�
				spFile->data = (int16_t*)malloc(data_size);
				unsigned char * buffers = (unsigned char *)malloc(spFile->pointSize * bits_per_sample);				
				fread(buffers, sizeof(unsigned char), spFile->pointSize * 2, fp);

				for (int i = 0; i < spFile->pointSize; i++) {
					unsigned char buffer[2];
					int16_t num;
					buffer[1] = buffers[i * 2];
					buffer[0] = buffers[i * 2 + 1];
					memcpy(&num, buffer, 2);
					spFile->data[i] = num;
				}
				
				spFile->size = size;
				spFile->format_tag = format_tag;
				spFile->channels = channels;
				spFile->block_align = block_align;
				spFile->bits_per_sample = bits_per_sample;
				spFile->format_length = format_length;
				spFile->sample_rate = sample_rate;
				spFile->avg_bytes_sec = avg_bytes_sec;
				spFile->data_size = data_size;
				
				
				cout << "channels " << channels << endl;
				cout << "sample_rate " << sample_rate << endl;
				cout << "bits_per_sample:" << bits_per_sample << endl;
				cout << "time " << spFile->pointSize / sample_rate << " seconds" << endl;
				cout << "pointSize " << spFile->pointSize << endl;
				cout << "ok" << endl;
			}
			else {
				cout << "Error: RIFF File but not a wave file\n";
			}
		}
		else {
			cout << "ERROR: not a RIFF file\n";
		}
	}
	fclose(fp);
}

void freeSource(WavData* data) {
	free(data->data);
}

int processSpeech(WavData& speechAudio) {
	int frameLen = 8000 / 1000 * 25; // 25ms Ϊ1֡ 
	int frameStepLen = 8000 / 1000 * 10; // 10ms��һ��֡ 
	int frameNum = (speechAudio.pointSize - frameLen) / frameStepLen + 1;

	

	if ((speechAudio.pointSize - frameLen) % frameStepLen != 0) { //����в�ȫ��֡ 
		frameNum += 1;
	}
	vector<double> meanEnergys(frameNum, 0.0);
	vector<double> zeroCroRates(frameNum, 0.0);
	
	cout << "frameNum:" << frameNum << endl;
	cout << "frameStepLen " << frameStepLen << endl;
	cout << "frameLen " << frameLen << endl;
	cout << "speechAudio.pointSize " << speechAudio.pointSize << endl;
	cout << speechAudio.data[speechAudio.pointSize-1] << endl;
	int seq = 0;

	for (int i = 0; i < speechAudio.pointSize; i += frameStepLen) {
		int frameBegin = i;
		int frameEnd = frameBegin + frameLen - 1;
		if (frameEnd > speechAudio.pointSize - 1) {
			frameEnd = speechAudio.pointSize - 1;
			i = speechAudio.pointSize - frameStepLen; 
		}
		meanEnergys[seq] = caculateMeanEnergy(speechAudio, frameBegin, frameEnd);
		zeroCroRates[seq] = caculateZeroCroRates(speechAudio, frameBegin, frameEnd);
		seq++;
		
	}
	vector<pair<int, int > > silencePair;
	vector<pair<int, int > > speechPair;
	silencePair = cutFrame(meanEnergys, zeroCroRates);
	speechPair = cutWavBySil(silencePair, speechAudio);
	genWav(speechPair, speechAudio);
	return 0;
}

vector<pair<int, int> > cutFrame(vector<double>  &Energys, vector<double> & zeroCroRates){
	int frameNum = Energys.size();
	vector<pair<int, int > > cutResult;
	double meanEnergy; double allEnergy = 0.0;
	double meanZero; double allZero = 0.0;
	for(int i = 0; i < frameNum; i++){
		allEnergy += Energys[i] / 1000;
		allZero += zeroCroRates[i]; 
	}
	meanEnergy = allEnergy / frameNum * 1000;
	meanZero = allZero / frameNum;
	cout <<"����ƽ������:" << meanEnergy<<endl;
	cout <<"����ƽ�������� " << meanZero << endl;
	double energyThreshold = 2.4864e+008 * 1.5;
	double zeroCroThreshold = 0.388074 ;
	
	// ʹ�������͹����ʻ��ֳ����� 
	vector<int> isSilence(frameNum, 0); //��¼�Ƿ�Ϊ����֡ 
	for(int i = 0; i < frameNum; i++){
		if(Energys[i] < energyThreshold && zeroCroRates[i] < zeroCroThreshold){
			isSilence[i] = 1;
		//	cout << "����� "<<i <<endl;
		}
	}
	
	//����50֡����40֡Ϊsilence, ��ô���ж�����Ϊ�����,
	int silNumThreshold = 48; 
	int silNumFrameThre = 50;
	
	
	int silVal = 0;
	int silHead, silTail;
	bool silStarted = false; //һ������Ƭ���Ƿ�ʼ 
	for(int i = 0; i < silNumFrameThre; i++){
		silVal += isSilence[i];	
	}
	//��ʼ�� 
	if(silVal >= silNumThreshold){
		if(silStarted == false){
			silHead = 0;
			silStarted = true;
		}
		else{
			;
		}
	}
	
	for(int i = 1; i < frameNum - silNumFrameThre + 1; i++){
		silVal += isSilence[i + silNumFrameThre -1];
		silVal -= isSilence[i - 1];  
		if(silVal >= silNumThreshold){ //�ж�Ϊ����ε������ 
			if(silStarted == false){
				silHead = i;
				silStarted = true;
			}
			else{
				silTail = i;
			}
		}
		else{  //����ж����Ǿ���� 
			if(silStarted == true){ //ǰ�����ж�Ϊ����� 
				silStarted = false;
				cout <<"�����:" << silHead << " ; " << silTail<<endl; 
				cutResult.push_back(pair<int, int> (silHead, silTail));
			}
		}
	}
	if(silStarted == true){
		cutResult.push_back(pair<int, int> (silHead, silTail));
	} 
	return cutResult;
}


vector<pair<int, int > >  cutWavBySil(vector<pair<int, int > > silencePair, WavData& speechAudio){
	//�õ�����Ķ� 
	int speechHead = 0; int speechTail;
	vector<pair<int, int > > speechPair;
	for(int i = 0; i < silencePair.size();i++){
		int silHead = silencePair[i].first * 80;
		int silTail = silencePair[i].second * 80 + 120;
		speechTail = silHead;
		speechPair.push_back(pair<int, int>(speechHead, speechTail));		
		speechHead = silTail;
	}
	speechPair.push_back(pair<int, int>(speechHead, speechAudio.pointSize -1)); 
	return speechPair; 
} 

/*����wav�ļ�*/
void genWav(vector<pair<int, int > > speechPair, WavData& speechAudio){
	for(int i = 0; i < speechPair.size(); i++){
		int head = speechPair[i].first;
		int tail = speechPair[i].second;
		int pointSize = tail - head + 1;
		if(pointSize < 1000) continue;
		WavData w;
		w.data = (int16_t*)malloc(pointSize * sizeof(int16_t));
		w.pointSize = pointSize;
	    for(int j = 0; j < pointSize; j++){
			w.data[j] = speechAudio.data[head + j]; 
		}
		w.format_tag = speechAudio.format_tag;
		w.channels = speechAudio.channels;
		w.block_align = speechAudio.block_align;
		w.bits_per_sample = speechAudio.bits_per_sample;
		w.format_length = speechAudio.format_length;
		w.sample_rate = speechAudio.sample_rate;
		w.avg_bytes_sec = speechAudio.avg_bytes_sec;
		w.data_size = pointSize * sizeof(int16_t);
		w.size = w.data_size + 44;
		
		ostringstream os;
		os << head / 80;
		string head_str = os.str();
		ostringstream os2;
		os2 << tail / 80;
		string tail_str = os2.str();
		if(head_str.size() < 6){
			int leftZeroNum = 6 - head_str.size();
			for (int i = 0; i < leftZeroNum; i++){
				head_str = "0" + head_str;
			}
		}
		
		if(tail_str.size() < 6){
			int leftZeroNum = 6 - tail_str.size();
			for (int i = 0; i < leftZeroNum; i++){
				tail_str = "0" + tail_str;
			}		
		}
		string s = "_" + head_str + "_" + tail_str;
		string s1 = "PHONE_001";
		string s2 = ".wav";
		
		writeWavFile((s1 + s + s2).c_str(), &w);
	}
}



double caculateMeanEnergy(WavData& speechAudio, int frameBegin, int frameEnd) {
	double sumEnergy = 0;

	for (int i = frameBegin; i <= frameEnd; i++) {
		sumEnergy += pow(double(speechAudio.data[i]), 2.0);
	}
	return sumEnergy / (frameEnd - frameBegin + 1);
}

double caculateZeroCroRates(WavData& speechAudio, int frameBegin, int frameEnd) {
	double zeros = 0;
	bool sign = sgn(speechAudio.data[frameBegin]);
	for (int i = frameBegin + 1; i <= frameEnd; i++) {
		int val = speechAudio.data[i];
		if (sgn(val) != sign) {
			zeros ++;
			sign = sgn(val);
		}
	}
	return zeros / (frameEnd - frameBegin + 1);
}

bool sgn(int16_t val) {
	if (val < 0) return false;
	else return true;
}

int main() {	
	WavData speechAudio;	
	const char* fname = "./PHONE_001.wav";
	loadWavFile(fname, &speechAudio);
	processSpeech(speechAudio);
	return 0;
}


