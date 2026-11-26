int max(int a, int b) {
    int r;
    if (a > b) {
        r = a;
    } else {
        r = b;
    }
    return r;
}

int main() {
    int x;
    x = max(7, 12);
    return x;
}
