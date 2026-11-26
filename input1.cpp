int pick(int a, int b, int c) {
    return a * 100 + b * 10 + c;
}

int main() {
    int x;
    x = 3;
    return pick(x, pick(0, 0, 2) + 1, x - 1);
}
