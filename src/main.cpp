#include<iostream>
int main() {
    int x = 3;
    int&& y = std::move(x);
    y = 5;
    std::cout << x;
    return 0;
}