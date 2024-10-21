#pragma once
#include <config.h>
#include <fstream> 
#include <memory>
#include <string>
#include <iostream>
#include <sstream>
#include <variant>

#ifdef HAVE_PARQUET
#include <arrow/io/file.h>
#include <arrow/util/config.h>

#include <parquet/api/reader.h>
#include <parquet/exception.h>
#include <parquet/stream_writer.h>
#endif


template <typename T, typename Base>
class StreamDevice {
public:

    // Constructor taking a reference (creates a new managed object)
    StreamDevice(T& stream) : myStream(std::make_unique<T>(stream)) {}

    // Constructor taking a pointer (takes ownership)
    StreamDevice(T* stream) : myStream(stream) {}

    // Constructor accepting a unique_ptr to T (transfers ownership)
    StreamDevice(std::unique_ptr<T> stream) : myStream(std::move(stream)) {}

    // Default constructor
    StreamDevice() : myStream(std::make_unique<T>()) {}

    // write method.
    template <typename ArgType>
    void write(const ArgType& t) {
        static_cast<Base*>(this)->writeImpl(t);
    }

    /// @brief Destructor
    virtual ~StreamDevice() = default;

    /// @brief is the stream ok
    /// @return true if the stream is ok
    virtual bool ok() {
        return myStream->good();
    }

    /// @brief set the precision
    /// @param precision
    virtual void setPrecision(int precision) {
        (*myStream) << std::setprecision(precision);
    }

    /// @brief get the precision
    /// @return the precision
    virtual int precision() {
        return static_cast<int>(myStream->precision());
    }

    /// @brief write a string to the stream
    /// @param s the string to write
    virtual void str(const std::string& s) {
        (*myStream) << s;
    }

    /// @brief write a string to the stream
    /// @param s the string to write
    virtual std::string str() = 0;

    /// @brief flush the stream
    /// @return this 
    virtual void flush() {
        myStream->flush();
    }

    /// @brief close the stream
    virtual void close() {
        flush();
    }

    /// @brief is the stream good
    /// @return true if the stream is good
    virtual bool good() {
        return myStream->good();
    }

    /// @brief implement a stream operator
    virtual operator std::ostream& () = 0;

    /// @brief write an endline to the stream
    /// @return this
    virtual StreamDevice<T, Base>& endLine() = 0;

    /// @brief set the output stream flags
    /// @param flags the flags to set
    virtual void setOSFlags(std::ios_base::fmtflags flags) = 0;

protected:
    // my Stream type
    std::unique_ptr<T> myStream;

};

class OStreamDevice : public StreamDevice<std::ostream, OStreamDevice> {

public:
    // // Constructor taking a reference (stores a reference to the stream)
    OStreamDevice(std::ostream& stream)
        : StreamDevice<std::ostream, OStreamDevice>(&stream){}

    // Constructor taking a pointer (takes ownership)
    OStreamDevice(std::ostream* stream)
        : StreamDevice<std::ostream, OStreamDevice>(stream) {}

    // Constructor accepting a unique_ptr to std::ostream (transfers ownership)
    OStreamDevice(std::unique_ptr<std::ostream> stream)
        : StreamDevice<std::ostream, OStreamDevice>(std::move(stream)) {}

    // Default constructor (creates a new ostringstream)
    OStreamDevice()
        : StreamDevice<std::ostream, OStreamDevice>(std::make_unique<std::ostringstream>()) {}

    // Copy constructor is deleted because std::ostream is not copyable
    OStreamDevice(const OStreamDevice&) = delete;


    std::string str() {
        // Use dynamic_cast to check if it's an ostringstream
        if (auto* oss = dynamic_cast<std::ostringstream*>(myStream.get())) {
            return oss->str();
        }

        // If not ostringstream, try to use the streambuf
        std::ostringstream ss;
        ss << myStream->rdbuf();
        return ss.str();
    }

    void str(const std::string& s) override {
        if (auto* oss = dynamic_cast<std::ostringstream*>(myStream.get())) {
            oss->str(s);
        }
        else {
            throw IOError("Cannot set string on non-ostringstream.");
        }
    }

    operator std::ostream& () override {
        return *myStream;
    }

    // overloading the endLine method
    OStreamDevice& endLine() override {
        (*myStream) << std::endl;
        return *this;
    }

    // overload the set iosflags method
    void setOSFlags(std::ios_base::fmtflags flags) override {
        myStream->setf(flags);
    }


    // The write method is implemented in the base class
    template <typename T>
    void writeImpl(T& t) {
        (*myStream) << t;
    }
};


typedef std::variant<std::unique_ptr<OStreamDevice>> StreamDeviceType;

// *********** Variant Helper Methods *****************
template <typename T>
StreamDeviceType& operator<<(StreamDeviceType& stream, const T& t) {
    std::visit([&t](auto&& arg) {
        arg->write(t);
        }, stream);
    return stream;
}

template <typename T>
T& getStreamDevice(StreamDeviceType& streamDevice) {
    if (auto res = std::get_if<std::unique_ptr<T>>(&streamDevice)) {
        return *res->get();  // Dereference the raw pointer obtained from the unique_ptr
    }
    throw IOError("Could not get stream device.");
}


inline int getStreamDevicePrecision(StreamDeviceType& streamDevice) {
    return std::visit([](auto&& arg) -> int {
        return arg->precision();
        }, streamDevice);
}
