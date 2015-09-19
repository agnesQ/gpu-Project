#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "common_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <iostream>
#include <fstream>
#include <vector>
#include "lodepng.h"
#include "lodepng.cpp"

using namespace std;

int NUM_BLOCKS;
int BLOCK_SIZE;

vector<unsigned char> image; 
vector<vector<unsigned char> > img;
vector<vector<unsigned char> > original_img;
unsigned char *img1;
unsigned long long *rowChecksum1;
unsigned long long *columnChecksum1;
unsigned char *OrigintotalRowSum;
unsigned char *OrigintotalColumnSum;
unsigned char *ScrambledRowSum;
unsigned char *ScrambledColSum;
unsigned width, height;
unsigned size;
unsigned num;

cudaError_t gpuchecksum(unsigned char *imge, unsigned long long *rowCS, unsigned long long *colCS, unsigned char *OriRowCS, unsigned char *OriColCS, unsigned char *ScrRowCS, unsigned char *ScrColCS, float* Runtimes);

__global__ void CheckSumKernelG(unsigned char *Gimg, unsigned long long *GrowCS, unsigned long long *GcolCS, unsigned char *GOriRowCS, unsigned char *GOriColCS, unsigned char *GScrRowCS, unsigned char *GScrColCS, unsigned height, unsigned width, unsigned size, unsigned num) {
	int gid = blockIdx.x*blockDim.x + threadIdx.x;
	int i, j, k, s;
	unsigned long long t, tmp;
	unsigned char count;
	int flag;
	
	flag = (gid/height)%height; 
	
	switch(flag){
	case 0:
			{i = gid;
			j = i/size;
			//s = 8 *(i%size);
			s = 8 *(size - 1 - i%size);
			t = (unsigned long long)255 << s;
			count = 0;
			for (k=0; k<num; ++k) {
				tmp = GcolCS[j*num+k] & t;
				count ^= (tmp >> s);
			}
			GOriRowCS[i] = count;
			break;}
	case 1:
			{i = gid - height;
			j = i/size;
			s = 8 *(size -1 - i%size);
			//s = 8 *(i%size);
			t = (unsigned long long)255 << s;
			count = 0;
			for (k = 0; k < num; ++k) {
				tmp = GrowCS[k*num +j] & t;
				count ^= (tmp >> s);
			}
			GOriColCS[i] = count;
			break;}

	case 2:
			{i = gid - (height+width);
			count = 0;
			for (j = 0; j < width; j++) {
				count ^= Gimg[i*width+j];
			}
			GScrRowCS[i] = count;
			break;}
			
	case 3:
			{i = gid - (2*height+width);
			count = 0;
			for (j = 0; j < height; j++) {
				count ^= Gimg[j*width+i];
			}
			GScrColCS[i] = count;
			break;}
	}

}

void Read_in(const char* filename)
{
  
  unsigned error = lodepng::decode(image, width, height, filename);
  if(error) cout << "decoder error " << error << ": " << lodepng_error_text(error) << endl;
  
  img1 = (unsigned char*)malloc(height*width*sizeof(unsigned char));

  for (int i = 0; i < height; ++i)
  {
  	vector<unsigned char> tmp;
  	long k = 4 * i * width;
  	for (int j = 0; j < 4 * width; j += 4)
  	{
  		tmp.push_back(image[k + j]);
  		img1[i*width+j/4] = image[k + j];
  	}
  	img.push_back(tmp);
  }
}



int main(int argc, char** argv)
{
	if (argc < 5)
	{
		cout << "input error" << endl;
		return 1;
	}
	Read_in(argv[1]);
	long ExeTime;
  	struct timeval t;
  	double start, end;
  	float GPURuntimes[4]; 
  	cudaError_t cudaStatus;

	
	size = argv[3][0] - '0';
	num = width / size;
	BLOCK_SIZE = atoi(argv[4]);
	if (2*(height+width)%BLOCK_SIZE==0) NUM_BLOCKS = 2*(height+width)/BLOCK_SIZE;
	else NUM_BLOCKS = 2*(height+width)/BLOCK_SIZE+1;
	int count = 0;
	int res = 0;
	vector<int> row(height, -1);
	vector<int> column(width, -1);
	
	rowChecksum1 = (unsigned long long*)malloc(num*num*sizeof(unsigned long long));
	columnChecksum1 = (unsigned long long*)malloc(num*num*sizeof(unsigned long long));

	int p = 0;
	ifstream fin;
	fin.open(argv[2]);
	unsigned long long i,j;
	char c;
	if (fin.is_open()) {
	  while (!fin.eof()) {
	    fin >> i >> c >> j;
	    rowChecksum1[p] = i;
	    columnChecksum1[p] = j;
	    p++;
	 }
	}	
	fin.close();

	OrigintotalRowSum = (unsigned char*)malloc(height*sizeof(unsigned char));
	ScrambledRowSum = (unsigned char*)malloc(height*sizeof(unsigned char));
	OrigintotalColumnSum = (unsigned char*)malloc(width*sizeof(unsigned char));
	ScrambledColSum = (unsigned char*)malloc(width*sizeof(unsigned char));

	cudaStatus = gpuchecksum(img1, rowChecksum1, columnChecksum1, OrigintotalRowSum, OrigintotalColumnSum, ScrambledRowSum, ScrambledColSum, GPURuntimes);
 	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "\n gpuchecksum failed!");
		return 1;
	}

	printf("\nKERNEL = ElimtKernelG ...\n");
	printf("Tfr CPU->GPU = %5.2f ms ... \nExecution = %5.2f ms ... \nTfr GPU->CPU = %5.2f ms   \n Total=%5.2f ms\n",GPURuntimes[1], GPURuntimes[2], GPURuntimes[3], GPURuntimes[0]);
	printf("-----------------------------------------------------------------\n");

	cudaStatus = cudaDeviceReset();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaDeviceReset failed!");
		return 1;
	}

	
	
	for (int i = 0; i < height; ++i)
	{
		count = 0;
		res = 0;
		for (int j = 0; j < height; ++j)
		{
			if (OrigintotalRowSum[i] == ScrambledRowSum[j])
			{
				count++;
				res = j;
			}
		}
		row[i] = res;

	}

	for (int i = 0; i < width; ++i)
	{
		count = 0;
		res = 0;
		for (int j = 0; j < width; j++)
		{
			if (OrigintotalColumnSum[i] == ScrambledColSum[j])
			{
				count++;
				res = j;
			}
		}
		column[i] = res;

	}

	original_img = img;
	for (int i = 0; i < height; ++i)
	{
		for (int j = 0; j < width; ++j)
		{
			original_img[i][j] = img[row[i]][column[j]];
		}
	}
	
	//encodeOneStep("original.png", width, height);

	FILE *fp;
	fp=fopen("recovered_result.txt", "w");
	for(int i=0; i<height; i++)
	{
		for(int j=0; j<width; j++)
		{
			fprintf(fp, "%d",original_img[i][j]);
			if(j!=width)
				fprintf(fp, ",");
		}
		fprintf(fp, "\n");
	}
	fclose(fp);


	free(img1);
	free(rowChecksum1);
	free(columnChecksum1);
	free(OrigintotalRowSum);
	free(OrigintotalColumnSum);
	free(ScrambledRowSum);
	free(ScrambledColSum);

	return 0;
}

cudaError_t gpuchecksum(unsigned char *imge, unsigned long long *rowCS, unsigned long long *colCS, unsigned char *OriRowCS, unsigned char *OriColCS, unsigned char *ScrRowCS, unsigned char *ScrColCS, float* Runtimes) {
	cudaEvent_t time1, time2, time3, time4;
  	
  	unsigned char *Gimg;
  	unsigned long long *GrowCS, *GcolCS;
  	unsigned char *GOriRowCS, *GOriColCS, *GScrRowCS, *GScrColCS;

	// Choose which GPU to run on, change this on a multi-GPU system.
	cudaError_t cudaStatus;
  	cudaStatus = cudaSetDevice(0);
  	if (cudaStatus != cudaSuccess) {
     fprintf(stderr, "cudaSetDevice failed!  Do you have a CUDA-capable GPU installed?");
	   goto Error;
  	}
 	cudaEventCreate(&time1);
  	cudaEventCreate(&time2);
  	cudaEventCreate(&time3);
  	cudaEventCreate(&time4);

	cudaEventRecord(time1, 0);
  // Allocate GPU buffer for inputs and outputs (sortenuse)

	cudaStatus = cudaMalloc((void**)&Gimg, height*width*sizeof(unsigned char));
  if (cudaStatus != cudaSuccess) {
         fprintf(stderr, "cudaMalloc failed!");
         goto Error;
  }

	cudaStatus = cudaMalloc((void**)&GrowCS, num*num*sizeof(unsigned long long));
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	cudaStatus = cudaMalloc((void**)&GcolCS, num*num*sizeof(unsigned long long));
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	cudaStatus = cudaMalloc((void**)&GOriRowCS, height*sizeof(unsigned char));
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	cudaStatus = cudaMalloc((void**)&GOriColCS, width*sizeof(unsigned char));
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	cudaStatus = cudaMalloc((void**)&GScrRowCS, height*sizeof(unsigned char));
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	cudaStatus = cudaMalloc((void**)&GScrColCS, width*sizeof(unsigned char));
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed!");
		goto Error;
	}

	// Copy input vectors from host memory to GPU buffers.
	cudaStatus = cudaMemcpy(Gimg, imge, height*width*sizeof(unsigned char), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaStatus = cudaMemcpy(GrowCS, rowCS, num*num*sizeof(unsigned long long), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaStatus = cudaMemcpy(GcolCS, colCS, num*num*sizeof(unsigned long long), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaEventRecord(time2, 0);

	CheckSumKernelG<<<NUM_BLOCKS, BLOCK_SIZE>>>(Gimg, GrowCS, GcolCS, GOriRowCS, GOriColCS, GScrRowCS, GScrColCS, height, width, size, num);

	cudaStatus = cudaGetLastError();
	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "error code %d (%s) launching kernel!\n", cudaStatus, cudaGetErrorString(cudaStatus));
		goto Error;
	}

	// cudaDeviceSynchronize waits for the kernel to finish, and returns
	// any errors encountered during the launch.
	cudaStatus = cudaDeviceSynchronize();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaDeviceSynchronize returned error code %d (%s) after launching addKernel!\n", cudaStatus, cudaGetErrorString(cudaStatus));
		goto Error;
	}


	cudaEventRecord(time3, 0);
	// Copy output (results) from GPU buffer to host (CPU) memory.
	cudaStatus = cudaMemcpy(OriRowCS, GOriRowCS, height*sizeof(unsigned char), cudaMemcpyDeviceToHost);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaStatus = cudaMemcpy(OriColCS, GOriColCS, width*sizeof(unsigned char), cudaMemcpyDeviceToHost);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaStatus = cudaMemcpy(ScrRowCS, GScrRowCS, height*sizeof(unsigned char), cudaMemcpyDeviceToHost);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaStatus = cudaMemcpy(ScrColCS, GScrColCS, width*sizeof(unsigned char), cudaMemcpyDeviceToHost);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto Error;
	}

	cudaEventRecord(time4, 0);
	cudaEventSynchronize(time1);
	cudaEventSynchronize(time2);
	cudaEventSynchronize(time3);
	cudaEventSynchronize(time4);

	float totalTime, tfrCPUtoGPU, tfrGPUtoCPU, kernelExecutionTime;

	cudaEventElapsedTime(&totalTime, time1, time4);
	cudaEventElapsedTime(&tfrCPUtoGPU, time1, time2);
	cudaEventElapsedTime(&kernelExecutionTime, time2, time3);
	cudaEventElapsedTime(&tfrGPUtoCPU, time3, time4);

	Runtimes[0] = totalTime;
	Runtimes[1] = tfrCPUtoGPU;
	Runtimes[2] = kernelExecutionTime;
	Runtimes[3] = tfrGPUtoCPU;

	Error:
	cudaFree(Gimg);
	cudaFree(GrowCS);
	cudaFree(GcolCS);
	cudaFree(GOriRowCS);
	cudaFree(GOriColCS);
	cudaFree(GScrRowCS);
	cudaFree(GScrColCS);
	cudaEventDestroy(time1);
	cudaEventDestroy(time2);
	cudaEventDestroy(time3);
	cudaEventDestroy(time4);

	return cudaStatus;
}