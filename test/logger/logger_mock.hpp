#include <gmock/gmock.h>

class Handler {

  public:

    virtual ~Handler() {};
    virtual void handle() = 0;
};

class MockHandler : public Handler {

  public:

    MOCK_METHOD(void, handle, (), (override));
};
