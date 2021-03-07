#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int get_bits(int value, int highest_bit, int lowest_bit)
{
    int mask = 0;
    for (int i = lowest_bit; i <= highest_bit; i++)
    {
        mask = mask + (0x0001 << i);
    }
    return (value & mask) >> lowest_bit;
}

int main()
{
    int test_number = 0x3C;
    printf("%X", get_bits(test_number, 5, 2));
    return 0;
}