#include<iostream>


int main()
{
    const int *p = new int[5]{1,2,0,0,4};
    std::cout << p[0] << "awd\n\r" << p[4];
    

    return 0;
}