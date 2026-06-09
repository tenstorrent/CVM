// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

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
