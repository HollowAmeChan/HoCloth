#pragma once

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Interface/IDataValidate.cs
class IDataValidate {
public:
    virtual ~IDataValidate() = default;

    virtual void DataValidate() = 0;
};

}  // namespace hocloth::mc2
