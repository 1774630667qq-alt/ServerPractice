#include <memory>
#include <iostream>

struct Foo {
    ~Foo() { std::cout << "Foo deleted\n"; }
};

void myDeleter(Foo* p) {
    std::cout << "Custom deleter called\n";
    delete p;
}

int main() {
    std::shared_ptr<Foo> p(new Foo(), [](Foo* ptr) {
        myDeleter(ptr);
    });
    return 0;
}
