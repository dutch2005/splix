#include <gtest/gtest.h>
#include "printer.h"
#include "ppdfile.h"
#include <string_view>
#include <utility>

// ────────────────────────────────────────────────────────────────────────────
// Helper Subclass for Testing Protected Members
// ────────────────────────────────────────────────────────────────────────────

class TestablePrinter : public Printer {
public:
    void setManufacturer(const std::string& m) { _manufacturer = m; }
    void setModel(const std::string& m) { _model = m; }
};

// ────────────────────────────────────────────────────────────────────────────
// Printer Class Tests
// ────────────────────────────────────────────────────────────────────────────

TEST(PrinterTest, Initialization) {
    TestablePrinter printer;
    printer.setManufacturer("Samsung");
    printer.setModel("ML-2010");
    
    EXPECT_EQ(printer.manufacturer(), "Samsung");
    EXPECT_EQ(printer.model(), "ML-2010");
}

TEST(PrinterTest, MoveSemantics) {
    TestablePrinter p1;
    p1.setManufacturer("Xerox");
    p1.setModel("Phaser 3117");
    
    Printer p2 = std::move(p1);
    
    EXPECT_EQ(p2.manufacturer(), "Xerox");
    // p1's strings should be empty after move
    EXPECT_TRUE(p1.manufacturer().empty());
}

// ────────────────────────────────────────────────────────────────────────────
// PPDFile Value Tests
// ────────────────────────────────────────────────────────────────────────────

TEST(PPDValueTest, StringSafety) {
    PPDValue v1("TestValue");
    PPDValue v2 = v1; // Copy assignment
    
    EXPECT_STREQ(static_cast<const char*>(v1), "TestValue");
    EXPECT_STREQ(static_cast<const char*>(v2), "TestValue");
    
    v1 = PPDValue("NewValue");
    EXPECT_STREQ(static_cast<const char*>(v1), "NewValue");
    EXPECT_STREQ(static_cast<const char*>(v2), "TestValue"); // v2 should remain unchanged
}
