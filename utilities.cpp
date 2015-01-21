#include "utilities.h"
#include <math.h>

// Modified from K&R C book
int reverse(char* output){
  int c, i, j;
  
  for(i = 0, j = strlen(output)-1; i < j; i++, j--){
    c = output[i];
    output[i] = output[j];
    output[j] = c;
  }
  
  return strlen(output);
}

int itoa(int n, char* output){
  int i, sign;
  
  if((sign = n) < 0){
    n = -n;
  }
  
  i = 0;
  do{
      output[i++] = n % 10 + '0';
  } while((n /= 10) > 0);
  if(sign < 0){
    output[i++] = '-';
  }
  output[i] = '\0';
  return reverse(output);
}

int dtostrf(float val, int precision, char *output) {
  int integer = (int) val;
  
  int decimal;
  if(integer < 0){
    decimal = (int)((integer - val) * pow(10, precision));
  } else {
    decimal = (int)((val - integer) * pow(10,precision));
  }
  
  int len = itoa(integer, output);
  output[len++] = '.';
  len += itoa(decimal, output + len);
  
  return len;
}
