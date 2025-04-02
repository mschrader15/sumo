/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.dev/sumo
// Copyright (C) 2012-2025 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    OutputFormatter.h
/// @author  Daniel Krajzewicz
/// @author  Michael Behrisch
/// @date    2012
///
// Abstract base class for output formatters
/****************************************************************************/
#pragma once
#include <config.h>

#include <string>
#include <vector>
#include <functional>
#include <utils/xml/SUMOXMLDefinitions.h>
#ifdef HAVE_PARQUET
#include "ParquetHelpers.h"
#include <parquet/stream_writer.h>

#endif


// ===========================================================================
// class declarations
// ===========================================================================
class Boundary;
class Position;
class PositionVector;
class RGBColor;

// ===========================================================================
// Type Erasure Base Class
// ===========================================================================
class Value {
    public:
        template <typename T, 
                  typename = typename std::enable_if<!std::is_same<typename std::decay<T>::type, Value>::value>::type>
        Value(T&& value) : impl_(new Model<typename std::decay<T>::type>(std::forward<T>(value))) {}
        
        // Copy/move constructors
        Value(const Value& other) : impl_(other.impl_->clone()) {}
        Value(Value&& other) noexcept : impl_(std::move(other.impl_)) {}
        
        // Assignment operators
        Value& operator=(const Value& other) {
            impl_.reset(other.impl_->clone());
            return *this;
        }
        
        Value& operator=(Value&& other) noexcept {
            impl_ = std::move(other.impl_);
            return *this;
        }
        
        // Interface methods
    #ifdef HAVE_PARQUET
        void write(parquet::StreamWriter& into) const {
            impl_->write(into);
        }
    #endif
        void write(std::ostream& into) const {
            impl_->write(into);
        }
        
    private:
        struct Concept {
            virtual ~Concept() = default;
    #ifdef HAVE_PARQUET
            virtual void write(parquet::StreamWriter&) const = 0;
    #endif
            virtual void write(std::ostream&) const = 0;
            virtual Concept* clone() const = 0;
        };
        
        // Type-specific implementation
        template <typename T>
        struct Model : Concept {
            explicit Model(T value) : value_(std::move(value)) {}
            
    #ifdef HAVE_PARQUET
            void write(parquet::StreamWriter& into) const override {
                into << convertToParquetType(value_);
            }
    #endif
            
            void write(std::ostream& into) const override {
                writeImpl(into, value_);
            }
            
            Concept* clone() const override {
                return new Model(value_);
            }
            
            // Helper methods for C++11 type dispatch instead of if constexpr
            template <typename U>
            typename std::enable_if<std::is_same<U, std::string>::value>::type
            writeImpl(std::ostream& into, const U& val) const {
                into << val;
            }
            
            template <typename U>
            typename std::enable_if<std::is_same<U, double>::value>::type
            writeImpl(std::ostream& into, const U& val) const {
    #ifdef HAVE_FMT
                fmt::print(into, "{:.{}f}", val, into.precision());
    #else
                into << std::fixed << std::setprecision(into.precision()) << val;
    #endif
            }
            
            template <typename U>
            typename std::enable_if<!std::is_same<U, std::string>::value && 
                                   !std::is_same<U, double>::value>::type
            writeImpl(std::ostream& into, const U& val) const {
                into << toString(val, into.precision());
            }
            
            T value_;
        };
        
        std::unique_ptr<Concept> impl_;
    };

// ===========================================================================
// class definitions
// ===========================================================================
/**
 * @class OutputFormatter
 * @brief Abstract base class for output formatters
 *
 * OutputFormatter format XML like output into the output stream.
 *  There are only two implementation at the moment, "normal" XML
 *  and binary XML.
 */
class OutputFormatter {
public:
    /// @brief Destructor
    virtual ~OutputFormatter() = default;


    /** @brief Writes an XML header with optional configuration
     *
     * If something has been written (myXMLStack is not empty), nothing
     *  is written and false returned.
     *
     * @param[in] into The output stream to use
     * @param[in] rootElement The root element to use
     * @param[in] attrs Additional attributes to save within the rootElement
     * @todo Check which parameter is used herein
     * @todo Describe what is saved
     */
    virtual bool writeXMLHeader(std::ostream& into, const std::string& rootElement,
        const std::map<SumoXMLAttr, std::string>& attrs,
        bool includeConfig = true) = 0;


    /** @brief Opens an XML tag
     *
     * An indentation, depending on the current xml-element-stack size, is written followed
     *  by the given xml element ("<" + xmlElement)
     * The xml element is added to the stack, then.
     *
     * @param[in] into The output stream to use
     * @param[in] xmlElement Name of element to open
     * @return The OutputDevice for further processing
     */
    virtual void openTag(std::ostream& into, const std::string& xmlElement) = 0;


    /** @brief Opens an XML tag
     *
     * Helper method which finds the correct string before calling openTag.
     *
     * @param[in] into The output stream to use
     * @param[in] xmlElement Id of the element to open
     */
    virtual void openTag(std::ostream& into, const SumoXMLTag& xmlElement) = 0;


    /** @brief Closes the most recently opened tag and optinally add a comment
     *
     * @param[in] into The output stream to use
     * @return Whether a further element existed in the stack and could be closed
     * @todo it is not verified that the topmost element was closed
     */
    virtual bool closeTag(std::ostream& into, const std::string& comment = "") = 0;

    virtual void writePreformattedTag(std::ostream& into, const std::string& val) = 0;

    virtual void writePadding(std::ostream& into, const std::string& val) = 0;

    virtual bool writeHeader(std::ostream& into, const SumoXMLTag& rootElement) = 0;

    virtual bool wroteHeader() const = 0;

    template <typename AttrType, typename T>
    void writeAttr(std::ostream& into, const AttrType& attr, const T& val) {
        Value wrapped_val(val);
        writeAttrImpl(into, toString(attr), wrapped_val);
    };

    virtual void writeAttrImpl(std::ostream& into, const std::string& attr, const Value& val) = 0;

};
