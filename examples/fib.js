var a = 0;
var b = 1;
// TODO: make it possible to generate fib numbers up until 1_000_000
// This will require implementing OP_NUMBER which operates on doubles
while (a < 32767) {
    console.log(a);
    var c = a + b;
    a = b;
    b = c;
}
