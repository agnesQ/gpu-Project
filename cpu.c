#define _GNU_SOURCE
#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>

unsigned int inputImage[96][96];
char imagename[100];
uint64_t pic_size = 96;
uint64_t box_size = 4;
uint64_t box_per_line = (96 / 4);
uint64_t box_num = (96 * 96 / (4 * 4));

void readImage(){
    IplImage* img_i = 0;
    FILE *fout;
    int M; //number of rows in image
    int N; //number of columns in image

    uchar *data_i;
    int i, j;
    int c;

    /*printf("Please enter a scrambled image filename: ");
    scanf("%s", imagename);

    img_i = cvLoadImage(imagename, 0);*/
    img_i = cvLoadImage("medium.png", 0);
    if(!img_i){
        printf("Could not load image file: %s\n", imagename);
        exit(EXIT_FAILURE);
    }
    M = img_i->height;
    N = img_i->width;

    data_i = (uchar *)img_i->imageData;
    for(i=0;i<M;i++){
        for(j=0;j<N;j++){
            c = data_i[i*img_i->widthStep+j];
            inputImage[i][j] = c;
        }
    }

    // show the image
    //cvShowImage("original_image", img_i );

    //waits till a key is pressed
    //cvWaitKey(0);

    fout = fopen("medium_matrix.txt", "w+");
    if(fout==NULL){
        printf("Open .txt file failed\n");
        exit(EXIT_FAILURE);
    }

    for(i=0;i<M;i++){
        for(j=0;j<N;j++){
            if(j==N-1)
                fprintf(fout, "%d\n", inputImage[i][j]);
            else
                fprintf(fout, "%d,", inputImage[i][j]);
        }
        //fprintf(fout, "\n");
    }

    fclose(fout);

    // release the image
    cvReleaseImage(&img_i);
}

typedef struct
{
	uint64_t* lst;
	uint64_t counter;
	uint64_t Getcounter(){return counter;}
	uint64_t *Getlst(){return lst;}
} Foo;
//This Foo used to record the possible options for a row or column

void Set_potentialArr(uint64_t counter, uint64_t *array, Foo *foo)
{
	((*foo).lst) = (uint64_t *) malloc(sizeof(uint64_t) * counter);
	((*foo).counter) = counter;
	uint64_t i;
	for(i = 0; i < counter; i++)
	{
		((*foo).lst)[i] = array[i];
	}
}

uint64_t Del_potentialArr(uint64_t position, Foo *foo)
{
	uint64_t i;
	uint64_t flag = 0;
	for(i = 0; i < ((*foo).counter); i++)
	{
		if(((*foo).lst)[i] == position)
		{
			flag = 1;
			break;
		}
	}
	if(flag == 0)
	{
		return 0;
	}
	uint64_t length = ((*foo).counter) - 1;
	uint64_t *temp = (uint64_t *) malloc(sizeof(uint64_t) * length);
	uint64_t temp_counter = 0;
	for(i = 0; i < ((*foo).counter); i++)
	{
		if(((*foo).lst)[i] != position)
		{
			temp[temp_counter] = ((*foo).lst)[i];
			temp_counter++;
		}
	}
	((*foo).counter) = length;
	free((*foo).lst);
	((*foo).lst) = temp;
	return 1; //drop possibility from the array
}

void Check_duplicate(Foo poss[])
{
	uint64_t gain = 1;
    uint64_t i;
    uint64_t j;
	while(gain != 0)
	{
		for(i = 0; i < pic_size; i++)
		{
			if(poss[i].counter == 1)
			{
				uint64_t position = ((poss[i]).lst)[0];
				for(j = 0; j < pic_size; j++)
				{
					if(i != j)
					{
						gain += Del_potentialArr(position, &(poss[j]));
					}
				}
			}
		}
        gain = 0;
	}
}

uint64_t Check_combination(uint64_t checksum, uint64_t option, uint64_t *combination, uint64_t *matrix, uint64_t ifcol)
{
	uint64_t i, j;
	for(i = 0; i < box_size; i++)
	{
		for(j = 0; j < box_size; j++)
		{
			if(i != j)
			{
				if(combination[i] == combination[j]) //not valid
				{
					return 0;// combination is not valid
				}
			}
		}
	}

	uint64_t total = -1;
	if(ifcol == 0){// 0 indicates the rows
		for(i = 0; i < box_size; i++)
		{
			if(total == -1)
			{
				total = matrix[combination[i] * pic_size + option];
			}
			else
			{
				total = total ^ matrix[combination[i] * pic_size + option];
			}
		}
	}
	else{// otherwise for the column
		for(i = 0; i < box_size; i++)
		{
			if(total == -1)
			{
				//total = matrix[combination[i] * pic_size + option];
				total = matrix[option * pic_size + combination[i]];
			}
			else
			{
				//total = total ^ matrix[combination[i] * pic_size + option];
				total = total ^ matrix[option * pic_size + combination[i]];
			}
		}
	}
	if(total == checksum)
	{
		return 1;// this column is possible on for this combination
	}
	return 0;
}

uint64_t Check_option(uint64_t checksum, uint64_t option, uint64_t start, Foo *poss, uint64_t *matrix, uint64_t ifcol)
{
	uint64_t *combination =  (uint64_t *) malloc(sizeof(uint64_t) * box_size);
	if(box_size == 2)
	{

		uint64_t a = 0, b = 0;
		while(a < ((poss[start]).counter -1 ))
        while(b < ((poss[start + 1]).counter -1))
        {
            combination[0] = ((poss[start]).lst)[a++];
            combination[1] = ((poss[start + 1]).lst)[b++];
            if(Check_combination(checksum, option, combination, matrix, ifcol))
            {
                free(combination);
                return 1;
            }
        }
		free(combination);
		return 0;//check every combination, if no one is possible, return 0
	}
	else if(box_size == 4)
	{
		uint64_t a = 0, b = 0, c = 0, d = 0;

		while (a < ((poss[start]).counter-1 ))
        while (b < ((poss[start + 1]).counter-1 ))
        while (c < ((poss[start + 2]).counter-1 ))
        while (d < ((poss[start + 3]).counter-1 ))
        {
        //	printf("a, b, c, d: %d, %d, %d, %d \n", a, b, c, d);
            combination[0] = ((poss[start]).lst)[a++];
            combination[1] = ((poss[start + 1]).lst)[b++];
            combination[2] = ((poss[start + 2]).lst)[c++];
            combination[3] = ((poss[start + 3]).lst)[d++];
            if(Check_combination(checksum, option, combination, matrix, ifcol))
            {
                free(combination);
                return 1;
            }
        }

		free(combination);
		return 0;
	}
	else
	{

		uint64_t a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0;
		while(a < ((poss[start]).counter -1 ))
        while(b < ((poss[start + 1]).counter -1 ))
        while(c < ((poss[start + 2]).counter -1 ))
		while(d < ((poss[start + 3]).counter -1 ))
		while(e < ((poss[start + 4]).counter -1 ))
		while(f < ((poss[start + 5]).counter -1 ))
		while(g < ((poss[start + 6]).counter -1 ))
		while(h < ((poss[start + 7]).counter -1 ))
		{
			combination[0] = ((poss[start]).lst)[a++];
            combination[1] = ((poss[start + 1]).lst)[b++];
            combination[2] = ((poss[start + 2]).lst)[c++];
            combination[3] = ((poss[start + 3]).lst)[d++];
            combination[4] = ((poss[start + 4]).lst)[e++];
            combination[5] = ((poss[start + 5]).lst)[f++];
            combination[6] = ((poss[start + 6]).lst)[g++];
            combination[7] = ((poss[start + 7]).lst)[h++];
			if(Check_combination(checksum, option, combination, matrix, ifcol))
			{
                free(combination);
                return 1;
			}
        }

		free(combination);
		return 0;
	}
}

void Boxes_row(uint64_t *row_check, uint64_t *col_check, uint64_t *matrix, Foo *row_poss, Foo *col_poss)
{
	int i,j;
	for(i = 0; i < box_num; i++) //check the ith boxes
	{
		//the column and row of this box start from column_start and row_start
		uint64_t column_start = (i % box_per_line) * box_size;
		uint64_t row_start = (i / box_per_line) * box_size;
		//check the rows of this box first
		for(j = 0; j < box_size; j++)
		{
			uint64_t checksum = row_check[i * box_size + j];
			uint64_t column = column_start + j;//the column is currently being checked
			uint64_t counter = (row_poss[column]).counter; //how many options for this column
			if(counter == 1) //only one option, no need to check
			{
				break;
			}
			uint64_t *options = (uint64_t *) malloc(sizeof(uint64_t) * counter);
			uint64_t k;
			for(k = 0; k < counter; k++)
			{
				options[k] = ((row_poss[column]).lst)[k];
			}
			uint64_t option;
			for(k = 0; k < counter; k++)
			{
				option = options[k];
				uint64_t result = Check_option(checksum, option, row_start, col_poss, matrix, 0);
				if(!result)
				{
					Del_potentialArr(option, row_poss + column);
					Check_duplicate(row_poss);
				}
			}
			free(options);
		}
	}
}

void Boxes_col(uint64_t *row_check, uint64_t *col_check, uint64_t *matrix, Foo *row_poss, Foo *col_poss)
{
	int i,j;
	for(i = 0; i < box_num; i++) //check the ith boxes
	{
		//the column and row of this box start from column_start and row_start
		uint64_t column_start = (i % box_per_line) * box_size;
		uint64_t row_start = (i / box_per_line) * box_size;
		//check the column of this box then
		for(j = 0; j < box_size; j++)
		{
			uint64_t checksum = col_check[( (i % box_per_line) * box_per_line + i / box_per_line) * box_size + j];
			uint64_t row = row_start + j; //the row is currently being checked
			uint64_t counter = (col_poss[row]).counter; //how many options for this column
			if(counter == 1)//only one option, no need to check
			{
				break;
			}
			uint64_t *options = (uint64_t *) malloc(sizeof(uint64_t) * counter);
			uint64_t k;
			for(k = 0; k < counter; k++)
			{
				options[k] = ((col_poss[row]).lst)[k];
			}
			uint64_t option;
			for(k = 0; k < counter; k++)
			{
				option = options[k];
				uint64_t result = Check_option(checksum, option, column_start, row_poss, matrix, 1);
				if(!result)
				{
					Del_potentialArr(option, col_poss + row);
					Check_duplicate(col_poss);
				}
			}
			free(options);
		}
	}

}

int main(uint64_t argc, char *argv[])
{
	uint64_t i; //the counter for looping
	uint64_t j; //the counter for looping
    uint64_t outputImage[pic_size][pic_size];

    uint64_t *matrix = (uint64_t *) malloc(sizeof(uint64_t) * (pic_size * pic_size));
	uint64_t *row_check = (uint64_t *) malloc(sizeof(uint64_t) * (box_num * box_size));
	uint64_t *col_check = (uint64_t *) malloc(sizeof(uint64_t) * (box_num * box_size));
	uint64_t *giant_row_checksum = (uint64_t *) malloc(sizeof(uint64_t) * pic_size);
	uint64_t *giant_col_checksum = (uint64_t *) malloc(sizeof(uint64_t) * pic_size);
	uint64_t *scrambled_row_checksum = (uint64_t *) malloc(sizeof(uint64_t) * pic_size);
	uint64_t *scrambled_col_checksum = (uint64_t *) malloc(sizeof(uint64_t) * pic_size);
	Foo *row_poss = (Foo *) malloc(sizeof(Foo) * pic_size);
	Foo *col_poss = (Foo *) malloc(sizeof(Foo) * pic_size);

    struct timeval t;
    double StartTime, EndTime;
    long TimeElapsed;

	readImage();

    FILE *fp;

    uint64_t counter = 0;
    char** tokens;
    char line[100];
	size_t len = 0;

    fp = fopen("medium_matrix.txt", "r");
	if (fp == NULL)
	{
		exit(EXIT_FAILURE);
	}
	while (fscanf(fp,"%s",line)!=EOF)
	{
		char *ptr=strtok(line,",");
		while (ptr)
		{
			uint64_t tmp=atoi(ptr);
			matrix[counter++]=tmp;
			ptr=strtok(NULL,",");
        }
	}

	//read checksums
	counter = 0;
	//line = NULL;
	len = 0;
	fp = fopen("medium.png_4x4.csv", "r");
	if (fp == NULL)
	{
		exit(EXIT_FAILURE);
	}
	while (fscanf(fp,"%s",line)!=EOF)
	{
		uint64_t row,col;
		sscanf(line,"%lld,%lld",&row,&col);
		for(i=0;i<box_size;i++)
		{
			row_check[counter * box_size + i] = (row >> ((box_size - i - 1) * 8))&255;
			col_check[((counter % box_per_line) * box_per_line + counter / box_per_line) * box_size + i] = (col >> ((box_size - i - 1) * 8))&255;
		}
		counter++;
	}

	//get the giant checksum from the check_sum of all the rows and columns
	//store in giant_row_checksum and giant_col_checksum
	for(i = 0; i < box_per_line; i++)
	{
		for(j = 0; j < pic_size; j++)
		{
			if(i == 0)
			{
				giant_row_checksum[j] = row_check[j];
				giant_col_checksum[j] = col_check[j];
			}
			else
			{
				giant_row_checksum[j] = giant_row_checksum[j] ^ row_check[i * pic_size + j];
				giant_col_checksum[j] = giant_col_checksum[j] ^ col_check[i * pic_size + j];
			}

		}
	}

	srand((unsigned int)time(NULL));
	gettimeofday(&t, NULL);
	StartTime = t.tv_sec*1000.00 + (t.tv_usec/1000.0);

	//get the xor of all the rows and columns from the scrambled picture
	//store in scrambled_row_checksum and scrambled_col_checksum
	for(i = 0; i < pic_size; i++)
	{
		for(j = 0; j < pic_size; j++)
		{
			if(i == 0)
			{
				scrambled_row_checksum[j] = matrix[j];
				scrambled_col_checksum[j] = matrix[j * pic_size];
			}
			else
			{
				scrambled_row_checksum[j] = scrambled_row_checksum[j] ^ matrix[i * pic_size + j];
				scrambled_col_checksum[j] = scrambled_col_checksum[j] ^ matrix[j * pic_size + i];
			}
		}
	}


	//check the checksum of original picture and the scrambled picture, to see which row or column is possible
	//store in Foo *row_poss and Foo *col_poss

	for(i = 0; i < pic_size; i++)
	{
		uint64_t row_array[256] ;
		uint64_t col_array[256] ;
		memset(row_array,-1,sizeof(row_array));
		memset(col_array,-1,sizeof(col_array));
		uint64_t row_counter = 0;
		uint64_t col_counter = 0;

		for(j = 0; j< pic_size; j++)
		{
			if(giant_row_checksum[i] == scrambled_row_checksum[j])
			{
				row_array[row_counter] = j;
				row_counter ++;
			}

			if(giant_col_checksum[i] == scrambled_col_checksum[j])
			{
				col_array[col_counter] = j;
				col_counter ++;
			}

		}
		Set_potentialArr(row_counter, row_array, row_poss + i);
		Set_potentialArr(col_counter, col_array, col_poss + i);
	}


	//drop the duplicated options of each row and column and check the combination on box base
	Check_duplicate(row_poss);
	Check_duplicate(col_poss);
	Boxes_row(row_check, col_check, matrix, row_poss, col_poss);
	Boxes_col(row_check, col_check, matrix, row_poss, col_poss);

	gettimeofday(&t, NULL);
	EndTime = t.tv_sec*1000.00 + (t.tv_usec/1000.0);
	TimeElapsed = (long) (EndTime-StartTime);
	printf(" DONE ... Start=%ld ... End=%ld",(long) StartTime,(long) EndTime);
	printf("\n\n========== TIME ELAPSED = %ld ms .. \n\n",TimeElapsed);

	for(i=0; i<pic_size; i++){
		for(j=0; j<pic_size; j++){
            if(col_poss[i].lst[0] * 96 + row_poss[j].lst[0]<pic_size*pic_size)
                outputImage[i][j] = matrix[col_poss[i].lst[0] * 96 + row_poss[j].lst[0]];
		}
	}

	fp=fopen("small_recovered.txt", "w+");
	for(i=0; i<96; i++){
		for(j=0; j<96; j++){
			fprintf(fp, "%d",outputImage[i][j]);
			if(j!=95)
				fprintf(fp, ",");
		}
		fprintf(fp, "\n");
	}
	fclose(fp);

	/*// Display the output image:
	Mat result = Mat(M, N, CV_8UC1, CPU_OutputArray);
	//show_image(result, "output image");
	// and save it to disk:
	string output_filename = "./images/output.png";
	if (!imwrite(output_filename, result)) {
		fprintf(stderr, "couldn't write output to disk!\n");
		free(CPU_OutputArray);
		exit(EXIT_FAILURE);
	}
	printf("Saved image '%s', size = %dx%d (dims = %d).\n", output_filename.c_str(), result.rows, result.cols, result.dims);*/

}