/*
   Z80 CPU assempbly language compiler for the ABC-80 micro board
   Charles Chiou
   Date: 8-12-95
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloc.h>

#define MAX_IDENTIFIER 16
#define HEX_NUMERALS "0123456789ABCDEF"

typedef struct _syntax_t {
    char argument[9],
	 hexcode[8];
    int  flag;          /* normal=0, n=1, nn=2, e=3, none=4 */
    struct _syntax_t *point_to_next_syntax;
    } _syntax;

typedef struct _opcode_t {
    char code_name[5];
    struct _opcode_t *point_to_next;
    _syntax *syntax;
    } _opcode;

typedef struct _identifier_t {
    char name[16];
    int address;
    } _identifier;

_syntax *index_syntax,  /* index to the processing syntax */
	*prev_syntax;   /* previously processed syntax */
_opcode *opcode,        /* first of the linked list */
	*prev_opcode,   /* previously processed command */
	*index_opcode;  /* index to the processing command */

_identifier identifier[MAX_IDENTIFIER];

int
determine_flag(const char codeline[])
{
#define CHECK_N 'n'
#define CHECK_E 'e'
#define CHECK_NONE "none"

int i,
    length_argument;

    length_argument=strlen(codeline);
    /* check for none */
    if(strcmp(codeline,CHECK_NONE)==0)
	return(4);
    /* check for e */
    for(i=0;i<length_argument;i++)
	if(codeline[i]==CHECK_E)
	    return(3);
    /* check for nn */
    for(i=0;i<length_argument-1;i++)
	if(codeline[i]==CHECK_N && codeline[i+1]==CHECK_N)
	    return(2);
    /* check for n */
    for(i=0;i<length_argument;i++)
	if(codeline[i]==CHECK_N)
	    return(1);
    return(0);
}

void
read_code_book()
{
#define CODE_BOOK "z80.opc"
#define END_OF_SUBLIST ":"

FILE *fp;
char codeline[10];
int check1=0,check2;

   if((fp=fopen(CODE_BOOK,"r"))==0)
       printf("Fatal error! The Z80 code file is missing.\n");
   else
   {
       while(fscanf(fp,"%s",codeline)!=EOF)
       {
	   index_opcode=malloc(sizeof(_opcode));
	   if(check1==0)
	   {
	       opcode=prev_opcode=index_opcode;
	       check1++;
	   }
	   else
	   {
	       prev_opcode->point_to_next=index_opcode;
	       prev_opcode=index_opcode;
	   }
	   strcpy(index_opcode->code_name,codeline);

/* sub routine: adding arguments */
    check2=0;   /* initialize the check for beginning of sub-linked-list */
    fscanf(fp,"%s\n",codeline);
    while(strcmp(codeline,END_OF_SUBLIST)!=0)
    {
	index_syntax=malloc(sizeof(_syntax));
	strcpy(index_syntax->argument,codeline);
	index_syntax->flag=determine_flag(codeline);
	fscanf(fp,"%s\n",codeline);
	strcpy(index_syntax->hexcode,codeline);
	if(check2==0)
	{
	    index_opcode->syntax=index_syntax;
	    prev_syntax=index_syntax;
	    check2++;
	}
	else
	{
	    prev_syntax->point_to_next_syntax=index_syntax;
	    prev_syntax=index_syntax;
	}
	index_syntax->point_to_next_syntax=NULL;
    }
/* end of subrouting*/

       }
       index_opcode->point_to_next=NULL;
   }
}

/* TEST
void
print_list()
{
	index_opcode=opcode;
	while(index_opcode->point_to_next!=NULL)
	{
	    printf("%s\n",index_opcode->code_name);
	    index_syntax=index_opcode->syntax;
	    while(index_syntax->point_to_next_syntax!=NULL)
	    {
		printf("%s ",index_syntax->argument);
		index_syntax=index_syntax->point_to_next_syntax;
	    }
	    index_opcode=index_opcode->point_to_next;
	}
}
*/

int
check_hex_address(char source_code[])           /* return 1 if wrong */
{
int i,j,check=0;

    if(strlen(source_code)!=4)
	return(1);
    for(i=0;i<4;i++)
	for(j=0;j<16;j++)
	    if(source_code[i]==HEX_NUMERALS[j])
		check++;
    if(check!=4)
	return(1);
    else
	return(0);
}

int
check_hex_data(const char data_code[])
{
int i,j,check=0;

    if(strlen(data_code)!=2)
	return(1);
    for(i=0;i<2;i++)
	for(j=0;j<16;j++)
	    if(data_code[i]==HEX_NUMERALS[j])
		check++;
    if(check!=2)
	return(1);
    else
	return(0);
}
char
dec_to_hex(const int decimal_value)
{
    if(decimal_value==0) return('0');    if(decimal_value==1) return('1');
    if(decimal_value==2) return('2');    if(decimal_value==3) return('3');
    if(decimal_value==4) return('4');    if(decimal_value==5) return('5');
    if(decimal_value==6) return('6');    if(decimal_value==7) return('7');
    if(decimal_value==8) return('8');    if(decimal_value==9) return('9');
    if(decimal_value==10) return('A');    if(decimal_value==11) return('B');
    if(decimal_value==12) return('C');    if(decimal_value==13) return('D');
    if(decimal_value==14) return('E');    if(decimal_value==15) return('F');
}

void
dec_to_hex_twobyte(const int decimal_value, char twobyte_hex[])
{
int numeral2,
    numeral1;

    numeral2=decimal_value/16;
    numeral1=decimal_value-numeral2/16;
    twobyte_hex[0]=dec_to_hex(numeral2);
    twobyte_hex[1]=dec_to_hex(numeral1);
}

void
dec_to_hex_fourbyte(const int decimal_value,char fourbyte_hex[])
{
int numeral4,
    numeral3,
    numeral2,
    numeral1;

    numeral4=decimal_value/4096;
    numeral3=(decimal_value-numeral4*4096)/256;
    numeral2=(decimal_value-numeral4*4096-numeral3*256)/16;
    numeral1=decimal_value-numeral4*4096-numeral3*256-numeral2*16;
    fourbyte_hex[0]=dec_to_hex(numeral4);
    fourbyte_hex[1]=dec_to_hex(numeral3);
    fourbyte_hex[2]=dec_to_hex(numeral2);
    fourbyte_hex[3]=dec_to_hex(numeral1);
}

int
hex_to_dec(const char hexcode)
{
    if(hexcode=='0') return(0);  if(hexcode=='1') return(1);
    if(hexcode=='2') return(2);  if(hexcode=='3') return(3);
    if(hexcode=='4') return(4);  if(hexcode=='5') return(5);
    if(hexcode=='6') return(6);  if(hexcode=='7') return(7);
    if(hexcode=='8') return(8);  if(hexcode=='9') return(9);
    if(hexcode=='A') return(10);  if(hexcode=='B') return(11);
    if(hexcode=='C') return(12);  if(hexcode=='D') return(13);
    if(hexcode=='E') return(14);  if(hexcode=='F') return(15);
}

int
twobyte_convert(const char twobyte_hex[])
{
    return(hex_to_dec(twobyte_hex[0])+16*hex_to_dec(twobyte_hex[1]));
}

int
fourbyte_convert(const char fourbyte_hex[4])
{
    return(hex_to_dec(fourbyte_hex[3])+16*hex_to_dec(fourbyte_hex[2])+256*hex_to_dec(fourbyte_hex[1])+4096*hex_to_dec(fourbyte_hex[0]));
}

int
separate_fields(const char command[],char field_one[], char field_two[])
{
int i=0,j,
    string_length;

    string_length=strlen(command);
    while(i<string_length || command[i]!=',')
	i++;
    if(command[i]!=',')
	return(1);
    else
    {
	  for(j=0;j<i;j++)
	      field_one[j]=command[j];
	  for(j=i+1;j<string_length;j++)
	      field_two[j]=command[j];
    }
    return(1);
}

int
check_special_command(const char command[],const char arg[],const int index_address,char data[])
{
int i;

char field_one[8],
     field_two[8];

    if(strcmp(command,"CALL")==0)
    {
	if(separate_fields(arg,field_one,field_two)==0)
	    return(0);
	else
	{
	    strcat(field_one,",nn");
	    for(index_syntax=index_opcode->syntax;index_syntax->point_to_next_syntax!=NULL && strcmp(field_one,index_syntax->argument)!=0;index_syntax=index_syntax->point_to_next_syntax);
	    if(strcmp(field_one,index_syntax->argument)!=0)
		return(0);
	    if(check_hex_address(field_two)==0)
	    {
		strcpy(data,index_syntax->hexcode);
		strcat(data,field_two);
	    }
	    else
	    {
		for(i=0;i<MAX_IDENTIFIER || strcmp(identifier[i].name,field_two)!=0;i++)
		if(strcmp(identifier[i].name,field_two)!=0)
		    return(0);
		dec_to_hex_fourbyte(identifier[i].address,field_two);
		strcpy(data,index_syntax->hexcode);
		strcat(data,field_two);
	    }
	    return(2);
	}
    }
    if(strcmp(command,"DJNZ")==0)
    {
	if(check_hex_data(arg)==0)
	{
	    strcpy(data,index_opcode->syntax->hexcode);
	    strcat(data,arg);
	    return(1);
	}
	else
	{
	    for(i=0;i<MAX_IDENTIFIER || strcmp(identifier[i].name,arg)!=0;i++)
	    if(strcmp(identifier[i].name,arg)!=0)
		return(0);
	    i=index_address-identifier[i].address;
	    if(i>128)
	    {
		printf("error:jump overflow in '%s %s'\n",command,arg);
		return(0);
	    }
	    dec_to_hex_twobyte(256-i,field_two);
	    strcpy(data,index_syntax->hexcode);
	    strcat(data,field_two);
	    return(1);
	}
    }
    if(strcmp(command,"JR")==0)
    {
	if(separate_fields(arg,field_one,field_two)==0)
	    return(0);
	else
	{
	    strcat(field_one,",n");
	    for(index_syntax=index_opcode->syntax;index_syntax->point_to_next_syntax!=NULL && strcmp(field_one,index_syntax->argument)!=0;index_syntax=index_syntax->point_to_next_syntax);
	    if(strcmp(field_one,index_syntax->argument)!=0)
		return(0);
	    if(check_hex_address(field_two)==0)
	    {
		strcpy(data,index_syntax->hexcode);
		strcat(data,field_two);
	    }
	    else
	    {
		for(i=0;i<MAX_IDENTIFIER || strcmp(identifier[i].name,field_two)!=0;i++)
		if(strcmp(identifier[i].name,field_two)!=0)
		    return(0);
		i=index_address-identifier[i].address;
		if(i>128)
		{
		    printf("error:jump overflow in '%s %s'\n",command,arg);
		    return(0);
		}
		dec_to_hex_twobyte(256-i,field_two);
		strcpy(data,index_syntax->hexcode);
		strcat(data,field_two);
	    }
	    return(2);
	}
    }
    if(strcmp(command,"JP")==0)
    {
	if(separate_fields(arg,field_one,field_two)==0)
	    return(0);
	else
	{
	    strcat(field_one,",nn");
	    for(index_syntax=index_opcode->syntax;index_syntax->point_to_next_syntax!=NULL && strcmp(field_one,index_syntax->argument)!=0;index_syntax=index_syntax->point_to_next_syntax);
	    if(strcmp(field_one,index_syntax->argument)!=0)
		return(0);
	    if(check_hex_address(field_two)==0)
	    {
		strcpy(data,index_syntax->hexcode);
		strcat(data,field_two);
	    }
	    else
	    {
		for(i=0;i<MAX_IDENTIFIER || strcmp(identifier[i].name,field_two)!=0;i++)
		if(strcmp(identifier[i].name,field_two)!=0)
		    return(0);
		dec_to_hex_fourbyte(identifier[i].address,field_two);
		strcpy(data,index_syntax->hexcode);
		strcat(data,field_two);
	    }
	    return(2);
	}
    }
    return(0);
}

int
compiler(const char inputfilename[12],const char outputfilename[12])
{
FILE *fpin,
     *fpout;

char hex_address[]="FFFF",
     data[8],
     code_buffer[32];

int identifier_count=0,
    special_count,
    index_address;

    if((fpin=fopen(inputfilename,"r"))==0)
    {
	printf("file not found");
	return(1);
    }
    if((fpout=fopen(outputfilename,"w"))==0)
    {
	printf("error opening '%s' for output!\n");
	return(1);
    }
    /* get the start address of the compiling prgram */
    fscanf(fpin,"%s",code_buffer);
    if(check_hex_address(code_buffer)==1)
    {
	printf("error:start address\n");
	return(1);
    }
    else
    {
	strncpy(hex_address,code_buffer,4);
	index_address=fourbyte_convert(hex_address);
    }
    /* start checking the code body */
    /* check the opcode first, then the argument */

    while(fscanf(fpin,"%s",code_buffer)!=EOF)
    {
	if(code_buffer[0]==';')         /* identifier */
	{
	    if(identifier_count>MAX_IDENTIFIER-1)
	    {
		printf("error:identifier exceeded limit of %d!\n",MAX_IDENTIFIER);
		return(1);
	    }
	    strcpy(identifier[identifier_count].name,code_buffer+1);
	    identifier[identifier_count].address=index_address;
	    identifier_count++;
	    fscanf(fpin,"%s",code_buffer);
	}
	for(index_opcode=opcode;index_opcode->point_to_next!=NULL && strcmp(code_buffer,index_opcode->code_name)!=0;index_opcode=index_opcode->point_to_next);
	if(strcmp(code_buffer,index_opcode->code_name)!=0)
	{
	    printf("error:%s\n",code_buffer);
	    return(1);
	}

	if(index_opcode->syntax->flag==4)  /* 'none' case */
	{
	    dec_to_hex_fourbyte(index_address,hex_address);
	    fprintf(fpout,"%s\t%s\t%s\n",hex_address,index_opcode->code_name,index_opcode->syntax->hexcode);
	    index_address+=strlen(index_opcode->syntax->hexcode)/2;
	}
	else
	{
	    fscanf(fpin,"%s",code_buffer);
	    for(index_syntax=index_opcode->syntax;index_syntax->point_to_next_syntax!=NULL && strcmp(code_buffer,index_syntax->argument)!=0;index_syntax=index_syntax->point_to_next_syntax);
	    if(strcmp(code_buffer,index_syntax->argument)==0)
	    {                           /* simple case */
		dec_to_hex_fourbyte(index_address,hex_address);
		fprintf(fpout,"%s\t%s t%s\t%s\n",hex_address,index_opcode->code_name,index_syntax->argument,index_syntax->hexcode);
		index_address+=strlen(index_syntax->hexcode)/2;
	    }
				/* harder case */
	    special_count=check_special_command(index_opcode->code_name,code_buffer,index_address,data);
	    if(special_count!=0)
	    {
		dec_to_hex_fourbyte(index_address,hex_address);
		fprintf("%s\t%s %s\t%s\n",hex_address,index_opcode->code_name,code_buffer,data);
		index_address+=(special_count+1);
	    }
	}
    }
    return(0);
}

main()
{
int success=1;
char inputfilename[12]="shit.z80",
     outputfilename[12]="test.z80";

    read_code_book();
    success=compiler(inputfilename,outputfilename);
    if(success==0)
	printf("compiled successfully!\n");
}

