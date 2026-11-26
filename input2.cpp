int fact(int n){
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}

int fib(int n){
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int gcd(int a, int b){
    if (b == 0) {
        return a;
    }
    return gcd(b, a % b);
}

int power(int base, int exp){
    int r = 1;
    while (exp > 0) {
        r = r * base;
        exp = exp - 1;
    }
    return r;
}

int f = fact(5);
int g = gcd(48, 18);
int p = power(2, 10);
