#include "mtpl/mtpl.hpp"
#include <iostream>

int main() {
    using namespace mtpl;
    
    struct TestPayload { int value = 0; };
    using TestEvent = Event<TestPayload>;
    
    TestEvent e{TestPayload{42}};
    std::cout << "MTPL imported successfully! Value: " << e.payload.value << std::endl;
    
    return 0;
}