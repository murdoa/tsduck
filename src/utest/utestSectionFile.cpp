//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------
//
//  TSUnit test suite for SectionFile (binary and XML).
//
//----------------------------------------------------------------------------

#include "tsSectionFile.h"
#include "tsPAT.h"
#include "tsPMT.h"
#include "tsCAT.h"
#include "tsTDT.h"
#include "tsCAIdentifierDescriptor.h"
#include "tsFileUtils.h"
#include "tsErrCodeReport.h"
#include "tsxmlElement.h"
#include "tsxmlDeclaration.h"
#include "tsDuckContext.h"
#include "tsCerrReport.h"
#include "tsunit.h"

#include "tables/psi_pat1_xml.h"
#include "tables/psi_pat1_sections.h"
#include "tables/psi_pmt_scte35_xml.h"
#include "tables/psi_pmt_scte35_sections.h"


//----------------------------------------------------------------------------
// The test fixture
//----------------------------------------------------------------------------

class SectionFileTest: public tsunit::Test
{
    TSUNIT_DECLARE_TEST(ConfigurationFile);
    TSUNIT_DECLARE_TEST(GenericDescriptor);
    TSUNIT_DECLARE_TEST(GenericShortTable);
    TSUNIT_DECLARE_TEST(GenericLongTable);
    TSUNIT_DECLARE_TEST(PAT1);
    TSUNIT_DECLARE_TEST(SCTE35);
    TSUNIT_DECLARE_TEST(Memory);
    TSUNIT_DECLARE_TEST(BuildSections);
    TSUNIT_DECLARE_TEST(MultiSectionsCAT);
    TSUNIT_DECLARE_TEST(MultiSectionsAtProgramLevelPMT);
    TSUNIT_DECLARE_TEST(MultiSectionsAtStreamLevelPMT);
    TSUNIT_DECLARE_TEST(Attribute);

public:
    virtual void beforeTest() override;
    virtual void afterTest() override;

private:
    // Unitary test for one table.
    void testTable(const char* name, const ts::UChar* ref_xml, const uint8_t* ref_sections, size_t ref_sections_size);
    ts::Report& report();
    fs::path _tempFileNameBin {};
    fs::path _tempFileNameXML {};
};

TSUNIT_REGISTER(SectionFileTest);


//----------------------------------------------------------------------------
// Initialization.
//----------------------------------------------------------------------------

// Test suite initialization method.
void SectionFileTest::beforeTest()
{
    if (_tempFileNameBin.empty() || _tempFileNameXML.empty()) {
        _tempFileNameBin = ts::TempFile(u".tmp.bin");
        _tempFileNameXML = ts::TempFile(u".tmp.xml");
    }
    fs::remove(_tempFileNameBin, &ts::ErrCodeReport());
    fs::remove(_tempFileNameXML, &ts::ErrCodeReport());
}

// Test suite cleanup method.
void SectionFileTest::afterTest()
{
    fs::remove(_tempFileNameBin, &ts::ErrCodeReport());
    fs::remove(_tempFileNameXML, &ts::ErrCodeReport());
}

ts::Report& SectionFileTest::report()
{
    if (tsunit::Test::debugMode()) {
        return CERR;
    }
    else {
        return NULLREP;
    }
}


//----------------------------------------------------------------------------
// Unitary tests from XML tables.
//----------------------------------------------------------------------------

#define TESTTABLE(name, data)                                                                     \
    void SectionFileTest::test##name()                                                            \
    {                                                                                             \
        testTable(#name, psi_##data##_xml, psi_##data##_sections, sizeof(psi_##data##_sections)); \
    }

TESTTABLE(PAT1, pat1)
TESTTABLE(SCTE35, pmt_scte35)

void SectionFileTest::testTable(const char* name, const ts::UChar* ref_xml, const uint8_t* ref_sections, size_t ref_sections_size)
{
    debug() << "SectionFileTest: Testing " << name << std::endl;

    // Convert XML reference content to binary tables.
    ts::DuckContext duck;
    ts::SectionFile xml(duck);
    TSUNIT_ASSERT(xml.parseXML(ref_xml));

    // Serialize binary tables to section data.
    std::ostringstream strm;
    TSUNIT_ASSERT(xml.saveBinary(strm));
    const std::string sections(strm.str());

    // In debug mode, analyze data before failing.
    if (debugMode() && (ref_sections_size != sections.size() || !ts::MemEqual(ref_sections, sections.data(), ref_sections_size))) {
        const size_t size = std::min(ref_sections_size, sections.size());
        // Search index of first differing byte.
        size_t diff = 0;
        while (diff < size && ref_sections[diff] == uint8_t(sections[diff])) {
            ++diff;
        }
        debug() << "Reference sections size: " << ref_sections_size << " bytes, generated sections: " << sections.size() << " bytes" << std::endl
                << "First differing bytes at index " << diff << std::endl;
        const uint32_t flags = ts::UString::HEXA | ts::UString::ASCII | ts::UString::OFFSET | ts::UString::BPL;
        if (diff > 0) {
            const size_t pre = std::min<size_t>(16, diff);
            debug() << "Before first difference:" << std::endl
                    << ts::UString::Dump(ref_sections + diff - pre, pre, flags, 2, 16, diff - pre);
        }
        if (diff < size) {
            const size_t post = std::min<size_t>(256, size - diff);
            debug() << "After first difference (reference):" << std::endl
                    << ts::UString::Dump(ref_sections + diff, post, flags, 2, 16, diff)
                    << "After first difference (reference):" << std::endl
                    << ts::UString::Dump(sections.data() + diff, post, flags, 2, 16, diff);
        }
    }

    // Compare serialized section data to reference section data.
    TSUNIT_EQUAL(ref_sections_size, sections.size());
    TSUNIT_EQUAL(0, ts::MemCompare(ref_sections, sections.data(), ref_sections_size));

    // Convert binary tables to XML.
    TSUNIT_EQUAL(ref_xml, xml.toXML());
}


//----------------------------------------------------------------------------
// Other unitary tests.
//----------------------------------------------------------------------------

TSUNIT_DEFINE_TEST(ConfigurationFile)
{
    const ts::UString conf(ts::SearchConfigurationFile(ts::SectionFile::XML_TABLES_MODEL));
    debug() << "SectionFileTest::testConfigurationFile: " << conf << std::endl;
    TSUNIT_ASSERT(fs::exists(conf));
}

TSUNIT_DEFINE_TEST(GenericDescriptor)
{
    static const uint8_t descData[] = {
        0x72,   // tag
        0x07,   // length
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
    };
    ts::Descriptor desc(descData, sizeof(descData));
    TSUNIT_ASSERT(desc.isValid());
    TSUNIT_EQUAL(0x72, desc.tag());
    TSUNIT_EQUAL(9, desc.size());
    TSUNIT_EQUAL(7, desc.payloadSize());

    ts::DuckContext duck;
    ts::DescriptorContext context(duck);
    ts::xml::Document doc(report());
    ts::xml::Element* root = doc.initialize(u"test");
    TSUNIT_ASSERT(root != nullptr);
    TSUNIT_ASSERT(desc.toXML(duck, root, context, true) != nullptr);

    ts::UString text(doc.toString());
    debug() << "SectionFileTest::testGenericDescriptor: " << text << std::endl;
    TSUNIT_EQUAL(
        u"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        u"<test>\n"
        u"  <generic_descriptor tag=\"0x72\">\n"
        u"    01 02 03 04 05 06 07\n"
        u"  </generic_descriptor>\n"
        u"</test>\n",
        text);

    ts::xml::Document doc2(report());
    TSUNIT_ASSERT(doc2.parse(text));
    root = doc2.rootElement();
    TSUNIT_ASSERT(root != nullptr);
    TSUNIT_EQUAL(u"test", root->name());

    ts::xml::ElementVector children;
    TSUNIT_ASSERT(root->getChildren(children, u"generic_descriptor", 1, 1));
    TSUNIT_EQUAL(1, children.size());

    ts::ByteBlock payload;
    TSUNIT_ASSERT(children[0]->getHexaText(payload));
    TSUNIT_EQUAL(7, payload.size());
    TSUNIT_ASSERT(payload == ts::ByteBlock(descData + 2, sizeof(descData) - 2));

    ts::Descriptor desc2;
    TSUNIT_ASSERT(desc2.fromXML(duck, children[0]));
    TSUNIT_EQUAL(0x72, desc2.tag());
    TSUNIT_EQUAL(7, desc2.payloadSize());
    TSUNIT_ASSERT(ts::ByteBlock(desc2.payload(), desc2.payloadSize()) == ts::ByteBlock(descData + 2, sizeof(descData) - 2));
}

TSUNIT_DEFINE_TEST(GenericShortTable)
{
    static const uint8_t refData[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

    const ts::SectionPtr refSection(new ts::Section(0xAB, false, refData, sizeof(refData)));
    TSUNIT_ASSERT(refSection != nullptr);
    TSUNIT_ASSERT(refSection->isValid());

    ts::DuckContext duck;
    ts::BinaryTable refTable;
    refTable.addSection(refSection);
    TSUNIT_ASSERT(refTable.isValid());
    TSUNIT_EQUAL(1, refTable.sectionCount());

    ts::BinaryTable::XMLOptions opt;
    opt.forceGeneric = true;

    ts::xml::Document doc(report());
    ts::xml::Element* root = doc.initialize(u"test");
    TSUNIT_ASSERT(root != nullptr);
    TSUNIT_ASSERT(refTable.toXML(duck, root, opt) != nullptr);

    ts::UString text(doc.toString());
    debug() << "SectionFileTest::testGenericShortTable: " << text << std::endl;
    TSUNIT_EQUAL(
        u"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        u"<test>\n"
        u"  <generic_short_table table_id=\"0xAB\" private=\"false\">\n"
        u"    01 02 03 04 05 06\n"
        u"  </generic_short_table>\n"
        u"</test>\n",
        text);

    ts::xml::Document doc2(report());
    TSUNIT_ASSERT(doc2.parse(text));
    root = doc2.rootElement();
    TSUNIT_ASSERT(root != nullptr);
    TSUNIT_EQUAL(u"test", root->name());

    ts::xml::ElementVector children;
    TSUNIT_ASSERT(root->getChildren(children, u"GENERIC_SHORT_TABLE", 1, 1));
    TSUNIT_EQUAL(1, children.size());

    ts::BinaryTable tab;
    TSUNIT_ASSERT(tab.fromXML(duck, children[0]));
    TSUNIT_ASSERT(tab.isValid());
    TSUNIT_ASSERT(tab.isShortSection());
    TSUNIT_ASSERT(!tab.isLongSection());
    TSUNIT_EQUAL(0xAB, tab.tableId());
    TSUNIT_EQUAL(1, tab.sectionCount());

    ts::SectionPtr sec(tab.sectionAt(0));
    TSUNIT_ASSERT(sec != nullptr);
    TSUNIT_ASSERT(sec->isValid());
    TSUNIT_EQUAL(0xAB, sec->tableId());
    TSUNIT_ASSERT(sec->isShortSection());
    TSUNIT_ASSERT(!sec->isPrivateSection());
    TSUNIT_EQUAL(6, sec->payloadSize());
    TSUNIT_ASSERT(ts::ByteBlock(sec->payload(), sec->payloadSize()) == ts::ByteBlock(refData, sizeof(refData)));
}

TSUNIT_DEFINE_TEST(GenericLongTable)
{
    static const uint8_t refData0[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    static const uint8_t refData1[] = {0x11, 0x12, 0x13, 0x14};

    ts::DuckContext duck;
    ts::BinaryTable refTable;
    refTable.addNewSection(0xCD, true, 0x1234, 7, true, 0, 0, refData0, sizeof(refData0));
    refTable.addNewSection(0xCD, true, 0x1234, 7, true, 1, 1, refData1, sizeof(refData1));
    TSUNIT_ASSERT(refTable.isValid());
    TSUNIT_ASSERT(refTable.isLongSection());
    TSUNIT_ASSERT(!refTable.isShortSection());
    TSUNIT_EQUAL(0xCD, refTable.tableId());
    TSUNIT_EQUAL(0x1234, refTable.tableIdExtension());
    TSUNIT_EQUAL(2, refTable.sectionCount());

    ts::BinaryTable::XMLOptions opt;
    opt.forceGeneric = true;

    ts::xml::Document doc(report());
    ts::xml::Element* root = doc.initialize(u"test");
    TSUNIT_ASSERT(root != nullptr);
    TSUNIT_ASSERT(refTable.toXML(duck, root, opt) != nullptr);

    ts::UString text(doc.toString());
    debug() << "SectionFileTest::testGenericLongTable: " << text << std::endl;
    TSUNIT_EQUAL(
        u"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        u"<test>\n"
        u"  <generic_long_table table_id=\"0xCD\" table_id_ext=\"0x1234\" version=\"7\" current=\"true\" private=\"true\">\n"
        u"    <section>\n"
        u"      01 02 03 04 05\n"
        u"    </section>\n"
        u"    <section>\n"
        u"      11 12 13 14\n"
        u"    </section>\n"
        u"  </generic_long_table>\n"
        u"</test>\n",
        text);

    ts::xml::Document doc2(report());
    TSUNIT_ASSERT(doc2.parse(text));
    root = doc2.rootElement();
    TSUNIT_ASSERT(root != nullptr);
    TSUNIT_EQUAL(u"test", root->name());

    ts::xml::ElementVector children;
    TSUNIT_ASSERT(root->getChildren(children, u"GENERIC_long_TABLE", 1, 1));
    TSUNIT_EQUAL(1, children.size());

    ts::BinaryTable tab;
    TSUNIT_ASSERT(tab.fromXML(duck, children[0]));
    TSUNIT_ASSERT(tab.isValid());
    TSUNIT_ASSERT(tab.isLongSection());
    TSUNIT_ASSERT(!tab.isShortSection());
    TSUNIT_EQUAL(0xCD, tab.tableId());
    TSUNIT_EQUAL(0x1234, tab.tableIdExtension());
    TSUNIT_EQUAL(2, tab.sectionCount());

    ts::SectionPtr sec(tab.sectionAt(0));
    TSUNIT_ASSERT(sec != nullptr);
    TSUNIT_ASSERT(sec->isValid());
    TSUNIT_EQUAL(0xCD, sec->tableId());
    TSUNIT_EQUAL(0x1234, sec->tableIdExtension());
    TSUNIT_EQUAL(7, sec->version());
    TSUNIT_ASSERT(!sec->isShortSection());
    TSUNIT_ASSERT(sec->isPrivateSection());
    TSUNIT_ASSERT(sec->isCurrent());
    TSUNIT_EQUAL(sizeof(refData0), sec->payloadSize());
    TSUNIT_ASSERT(ts::ByteBlock(sec->payload(), sec->payloadSize()) == ts::ByteBlock(refData0, sizeof(refData0)));

    sec = tab.sectionAt(1);
    TSUNIT_ASSERT(sec != nullptr);
    TSUNIT_ASSERT(sec->isValid());
    TSUNIT_EQUAL(0xCD, sec->tableId());
    TSUNIT_EQUAL(0x1234, sec->tableIdExtension());
    TSUNIT_EQUAL(7, sec->version());
    TSUNIT_ASSERT(!sec->isShortSection());
    TSUNIT_ASSERT(sec->isPrivateSection());
    TSUNIT_ASSERT(sec->isCurrent());
    TSUNIT_EQUAL(sizeof(refData1), sec->payloadSize());
    TSUNIT_ASSERT(ts::ByteBlock(sec->payload(), sec->payloadSize()) == ts::ByteBlock(refData1, sizeof(refData1)));
}

TSUNIT_DEFINE_TEST(BuildSections)
{
    ts::DuckContext duck;

    // Build a PAT with 2 sections.
    ts::PAT pat(7, true, 0x1234);
    TSUNIT_EQUAL(ts::PID_NIT, pat.nit_pid);
    for (uint16_t srv = 3; srv < ts::MAX_PSI_LONG_SECTION_PAYLOAD_SIZE / 4 + 16; ++srv) {
        pat.pmts[srv] = ts::PID(srv + 2);
    }

    // Serialize the PAT.
    ts::BinaryTablePtr patBin(new(ts::BinaryTable));
    TSUNIT_ASSERT(patBin != nullptr);
    pat.serialize(duck, *patBin);
    TSUNIT_ASSERT(patBin->isValid());
    TSUNIT_EQUAL(2, patBin->sectionCount());

    // Build a section file.
    ts::SectionFile file(duck);
    file.add(patBin);
    TSUNIT_EQUAL(1, file.tables().size());
    TSUNIT_EQUAL(2, file.sections().size());
    TSUNIT_EQUAL(0, file.orphanSections().size());

    file.add(patBin->sectionAt(0));
    TSUNIT_EQUAL(1, file.tables().size());
    TSUNIT_EQUAL(3, file.sections().size());
    TSUNIT_EQUAL(1, file.orphanSections().size());

    file.add(patBin->sectionAt(1));
    TSUNIT_EQUAL(2, file.tables().size());
    TSUNIT_EQUAL(4, file.sections().size());
    TSUNIT_EQUAL(0, file.orphanSections().size());

    // Build a TDT (short section).
    const ts::Time tdtTime(ts::Time::Fields(2017, 12, 25, 14, 55, 27));
    ts::TDT tdt(tdtTime);

    ts::BinaryTablePtr tdtBin(new(ts::BinaryTable));
    TSUNIT_ASSERT(tdtBin != nullptr);
    tdt.serialize(duck, *tdtBin);
    TSUNIT_ASSERT(tdtBin->isValid());
    TSUNIT_EQUAL(1, tdtBin->sectionCount());

    file.add(tdtBin);
    TSUNIT_EQUAL(3, file.tables().size());
    TSUNIT_EQUAL(5, file.sections().size());
    TSUNIT_EQUAL(0, file.orphanSections().size());

    // Save files.
    debug() << "SectionFileTest::testBuildSections: saving " << _tempFileNameBin << std::endl;
    TSUNIT_ASSERT(!fs::exists(_tempFileNameBin));
    TSUNIT_ASSERT(file.saveBinary(_tempFileNameBin));
    TSUNIT_ASSERT(fs::exists(_tempFileNameBin));

    debug() << "SectionFileTest::testBuildSections: saving " << _tempFileNameXML << std::endl;
    TSUNIT_ASSERT(!fs::exists(_tempFileNameXML));
    TSUNIT_ASSERT(file.saveXML(_tempFileNameXML));
    TSUNIT_ASSERT(fs::exists(_tempFileNameXML));

    // Reload files.
    ts::SectionFile binFile(duck);
    binFile.setCRCValidation(ts::CRC32::CHECK);
    TSUNIT_ASSERT(binFile.loadBinary(_tempFileNameBin));
    TSUNIT_EQUAL(3, binFile.tables().size());
    TSUNIT_EQUAL(5, binFile.sections().size());
    TSUNIT_EQUAL(0, binFile.orphanSections().size());

    ts::SectionFile xmlFile(duck);
    TSUNIT_ASSERT(xmlFile.loadXML(_tempFileNameXML));
    TSUNIT_EQUAL(3, xmlFile.tables().size());
    TSUNIT_EQUAL(5, xmlFile.sections().size());
    TSUNIT_EQUAL(0, xmlFile.orphanSections().size());

    for (size_t i = 0; i < file.tables().size(); ++i) {
        TSUNIT_ASSERT(*file.tables()[i] == *binFile.tables()[i]);
        TSUNIT_ASSERT(*file.tables()[i] == *xmlFile.tables()[i]);
    }
    for (size_t i = 0; i < file.sections().size(); ++i) {
        TSUNIT_ASSERT(*file.sections()[i] == *binFile.sections()[i]);
        TSUNIT_ASSERT(*file.sections()[i] == *xmlFile.sections()[i]);
    }

    ts::PAT binPAT(duck, *binFile.tables()[0]);
    TSUNIT_ASSERT(binPAT.isValid());
    TSUNIT_EQUAL(7, binPAT.version());
    TSUNIT_EQUAL(0x1234, binPAT.ts_id);
    TSUNIT_EQUAL(ts::PID_NIT, binPAT.nit_pid);
    TSUNIT_ASSERT(binPAT.pmts == pat.pmts);

    ts::PAT xmlPAT(duck, *xmlFile.tables()[0]);
    TSUNIT_ASSERT(xmlPAT.isValid());
    TSUNIT_EQUAL(7, xmlPAT.version());
    TSUNIT_EQUAL(0x1234, xmlPAT.ts_id);
    TSUNIT_EQUAL(ts::PID_NIT, xmlPAT.nit_pid);
    TSUNIT_ASSERT(xmlPAT.pmts == pat.pmts);

    ts::TDT binTDT(duck, *binFile.tables()[2]);
    TSUNIT_ASSERT(tdtTime == binTDT.utc_time);

    ts::TDT xmlTDT(duck, *xmlFile.tables()[2]);
    TSUNIT_ASSERT(tdtTime == xmlTDT.utc_time);
}

TSUNIT_DEFINE_TEST(MultiSectionsCAT)
{
    ts::DuckContext duck;
    ts::CAT cat1;

    TSUNIT_ASSERT(cat1.isValid());
    TSUNIT_ASSERT(!cat1.isPrivate());
    TSUNIT_EQUAL(ts::TID_CAT, cat1.tableId());
    TSUNIT_EQUAL(0xFFFF, cat1.tableIdExtension());
    TSUNIT_ASSERT(cat1.descs.empty());

    // Add 300 10-byte descriptors => 3000 bytes => 3 sections.
    // One CAT section = 1024 bytes max, 1012 payload max => 101 descriptors per section.
    uint16_t counter = 0;
    for (size_t di = 0; di < 300; ++di) {
        cat1.descs.add(duck, ts::CAIdentifierDescriptor({counter, uint16_t(counter+1), uint16_t(counter+2), uint16_t(counter+3)}));
        TSUNIT_EQUAL(di + 1, cat1.descs.size());
        TSUNIT_EQUAL(10, cat1.descs[di].size());
        counter += 4;
    }

    ts::BinaryTable bin;
    cat1.serialize(duck, bin);

    TSUNIT_ASSERT(bin.isValid());
    TSUNIT_ASSERT(!bin.isShortSection());
    TSUNIT_EQUAL(ts::TID_CAT,bin.tableId());
    TSUNIT_EQUAL(0xFFFF, bin.tableIdExtension());
    TSUNIT_EQUAL(3, bin.sectionCount());
    TSUNIT_EQUAL(1022, bin.sectionAt(0)->size());
    TSUNIT_EQUAL(1010, bin.sectionAt(0)->payloadSize());
    TSUNIT_EQUAL(1022, bin.sectionAt(1)->size());
    TSUNIT_EQUAL(1010, bin.sectionAt(1)->payloadSize());
    TSUNIT_EQUAL(992, bin.sectionAt(2)->size());
    TSUNIT_EQUAL(980, bin.sectionAt(2)->payloadSize());

    ts::CAT cat2(duck, bin);
    TSUNIT_ASSERT(cat2.isValid());
    TSUNIT_ASSERT(!cat2.isPrivate());
    TSUNIT_EQUAL(ts::TID_CAT, cat2.tableId());
    TSUNIT_EQUAL(0xFFFF, cat2.tableIdExtension());
    TSUNIT_EQUAL(300, cat2.descs.size());

    counter = 0;
    for (size_t di = 0; di < cat2.descs.size(); ++di) {
        ts::CAIdentifierDescriptor desc(duck, cat2.descs[di]);
        TSUNIT_ASSERT(desc.isValid());
        TSUNIT_EQUAL(4, desc.casids.size());
        for (size_t ii = 0; ii < desc.casids.size(); ++ii) {
            TSUNIT_EQUAL(counter, desc.casids[ii]);
            counter++;
        }
    }
}

TSUNIT_DEFINE_TEST(MultiSectionsAtProgramLevelPMT)
{
    ts::DuckContext duck;
    ts::PMT pmt1;

    pmt1.service_id = 0x5678;
    pmt1.pcr_pid = 0x1234;

    TSUNIT_ASSERT(pmt1.isValid());
    TSUNIT_ASSERT(!pmt1.isPrivate());
    TSUNIT_EQUAL(ts::TID_PMT, pmt1.tableId());
    TSUNIT_EQUAL(0x5678, pmt1.tableIdExtension());
    TSUNIT_ASSERT(pmt1.descs.empty());
    TSUNIT_ASSERT(pmt1.streams.empty());

    // Add 202 10-byte descriptors => 2020 bytes => 3 sections.
    // One PSI section = 1024 bytes max, 1012 payload max, incl. 4-byte fixed part => 100 descriptors per section.
    uint16_t counter = 0;
    for (size_t di = 0; di < 202; ++di) {
        pmt1.descs.add(duck, ts::CAIdentifierDescriptor({counter, uint16_t(counter+1), uint16_t(counter+2), uint16_t(counter+3)}));
        counter += 4;
    }

    // Add only one stream, with one descriptor.
    const ts::PID es_pid = 100;
    pmt1.streams[es_pid].stream_type = 0xAB;
    pmt1.streams[es_pid].descs.add(duck, ts::CAIdentifierDescriptor({counter, uint16_t(counter+1), uint16_t(counter+2), uint16_t(counter+3)}));

    ts::BinaryTable bin;
    pmt1.serialize(duck, bin);

    TSUNIT_ASSERT(bin.isValid());
    TSUNIT_ASSERT(!bin.isShortSection());
    TSUNIT_EQUAL(ts::TID_PMT,bin.tableId());
    TSUNIT_EQUAL(0x5678, bin.tableIdExtension());
    TSUNIT_EQUAL(3, bin.sectionCount());
    TSUNIT_EQUAL(1016, bin.sectionAt(0)->size());
    TSUNIT_EQUAL(1004, bin.sectionAt(0)->payloadSize());
    TSUNIT_EQUAL(1016, bin.sectionAt(1)->size());
    TSUNIT_EQUAL(1004, bin.sectionAt(1)->payloadSize());
    TSUNIT_EQUAL(51, bin.sectionAt(2)->size());
    TSUNIT_EQUAL(39, bin.sectionAt(2)->payloadSize());

    ts::PMT pmt2(duck, bin);
    TSUNIT_ASSERT(pmt2.isValid());
    TSUNIT_ASSERT(!pmt2.isPrivate());
    TSUNIT_EQUAL(ts::TID_PMT, pmt2.tableId());
    TSUNIT_EQUAL(0x5678, pmt2.tableIdExtension());
    TSUNIT_EQUAL(0x1234, pmt2.pcr_pid);
    TSUNIT_EQUAL(202, pmt2.descs.size());

    counter = 0;
    for (size_t di = 0; di < pmt2.descs.size(); ++di) {
        ts::CAIdentifierDescriptor desc(duck, pmt2.descs[di]);
        TSUNIT_ASSERT(desc.isValid());
        TSUNIT_EQUAL(4, desc.casids.size());
        for (size_t ii = 0; ii < desc.casids.size(); ++ii) {
            TSUNIT_EQUAL(counter, desc.casids[ii]);
            counter++;
        }
    }

    TSUNIT_EQUAL(1, pmt2.streams.size());
    TSUNIT_EQUAL(100, pmt2.streams.begin()->first);
    const ts::PMT::Stream& es(pmt2.streams.begin()->second);
    TSUNIT_EQUAL(0xAB, es.stream_type);
    TSUNIT_EQUAL(1, es.descs.size());

    ts::CAIdentifierDescriptor desc(duck, es.descs[0]);
    TSUNIT_ASSERT(desc.isValid());
    TSUNIT_EQUAL(4, desc.casids.size());
    for (size_t ii = 0; ii < desc.casids.size(); ++ii) {
        TSUNIT_EQUAL(counter, desc.casids[ii]);
        counter++;
    }
}

TSUNIT_DEFINE_TEST(MultiSectionsAtStreamLevelPMT)
{
    ts::DuckContext duck;
    ts::PMT pmt1;

    pmt1.service_id = 0x5678;
    pmt1.pcr_pid = 0x1234;

    TSUNIT_ASSERT(pmt1.isValid());
    TSUNIT_ASSERT(!pmt1.isPrivate());
    TSUNIT_EQUAL(ts::TID_PMT, pmt1.tableId());
    TSUNIT_EQUAL(0x5678, pmt1.tableIdExtension());
    TSUNIT_ASSERT(pmt1.descs.empty());
    TSUNIT_ASSERT(pmt1.streams.empty());

    // Add 3 10-byte descriptors at program level.
    // First section initial size: 34 bytes. Subsequent sections: 4 bytes.
    uint16_t counter = 0;
    for (size_t di = 0; di < 3; ++di) {
        pmt1.descs.add(duck, ts::CAIdentifierDescriptor({counter, uint16_t(counter+1), uint16_t(counter+2), uint16_t(counter+3)}));
        counter += 4;
    }

    // Add 90 streams, with 2 descriptors => 25 bytes per stream.
    // One PSI section = 1024 bytes max, 1012 payload max.
    // First section payload: 34 bytes + 39 x 25 bytes = 1009 bytes
    // Second section payload: 4 bytes + 40 x 25 bytes = 1004 bytes
    // Third section payload: 4 bytes + 11 x 25 bytes = 279 bytes
    ts::PID es_pid = 50;
    uint8_t stype = 0;
    for (size_t si = 0; si < 90; ++si) {
        pmt1.streams[es_pid].stream_type = stype++;
        pmt1.streams[es_pid].descs.add(duck, ts::CAIdentifierDescriptor({counter, uint16_t(counter+1), uint16_t(counter+2), uint16_t(counter+3)}));
        counter += 4;
        pmt1.streams[es_pid].descs.add(duck, ts::CAIdentifierDescriptor({counter, uint16_t(counter+1), uint16_t(counter+2), uint16_t(counter+3)}));
        counter += 4;
        es_pid++;
    }

    ts::BinaryTable bin;
    pmt1.serialize(duck, bin);

    TSUNIT_ASSERT(bin.isValid());
    TSUNIT_ASSERT(!bin.isShortSection());
    TSUNIT_EQUAL(ts::TID_PMT,bin.tableId());
    TSUNIT_EQUAL(0x5678, bin.tableIdExtension());
    TSUNIT_EQUAL(3, bin.sectionCount());
    TSUNIT_EQUAL(1021, bin.sectionAt(0)->size());
    TSUNIT_EQUAL(1009, bin.sectionAt(0)->payloadSize());
    TSUNIT_EQUAL(1016, bin.sectionAt(1)->size());
    TSUNIT_EQUAL(1004, bin.sectionAt(1)->payloadSize());
    TSUNIT_EQUAL(291, bin.sectionAt(2)->size());
    TSUNIT_EQUAL(279, bin.sectionAt(2)->payloadSize());

    ts::PMT pmt2(duck, bin);
    TSUNIT_ASSERT(pmt2.isValid());
    TSUNIT_ASSERT(!pmt2.isPrivate());
    TSUNIT_EQUAL(ts::TID_PMT, pmt2.tableId());
    TSUNIT_EQUAL(0x5678, pmt2.tableIdExtension());
    TSUNIT_EQUAL(0x1234, pmt2.pcr_pid);
    TSUNIT_EQUAL(3, pmt2.descs.size());

    counter = 0;
    es_pid = 50;
    stype = 0;

    for (size_t di = 0; di < pmt2.descs.size(); ++di) {
        ts::CAIdentifierDescriptor desc(duck, pmt2.descs[di]);
        TSUNIT_ASSERT(desc.isValid());
        TSUNIT_EQUAL(4, desc.casids.size());
        for (size_t ii = 0; ii < desc.casids.size(); ++ii) {
            TSUNIT_EQUAL(counter, desc.casids[ii]);
            counter++;
        }
    }

    TSUNIT_EQUAL(90, pmt2.streams.size());
    for (const auto& si : pmt2.streams) {
        TSUNIT_EQUAL(es_pid, si.first);
        es_pid++;
        TSUNIT_EQUAL(stype, si.second.stream_type);
        stype++;
        TSUNIT_EQUAL(2, si.second.descs.size());
        for (size_t di = 0; di < si.second.descs.size(); ++di) {
            ts::CAIdentifierDescriptor desc(duck, si.second.descs[di]);
            TSUNIT_ASSERT(desc.isValid());
            TSUNIT_EQUAL(4, desc.casids.size());
            for (size_t ii = 0; ii < desc.casids.size(); ++ii) {
                TSUNIT_EQUAL(counter, desc.casids[ii]);
                counter++;
            }
        }
    }
}

TSUNIT_DEFINE_TEST(Memory)
{
    ts::ByteBlock input(5);
    input.append(psi_pat1_sections, sizeof(psi_pat1_sections));
    input.append(psi_pmt_scte35_sections, sizeof(psi_pmt_scte35_sections));
    input.appendInt24(0);
    TSUNIT_EQUAL(5 + 32 + 55 + 3, input.size());

    ts::DuckContext duck;
    ts::SectionFile sf1(duck);
    TSUNIT_ASSERT(sf1.loadBuffer(input, 5, 87));
    TSUNIT_EQUAL(87, sf1.binarySize());
    TSUNIT_EQUAL(2, sf1.sectionsCount());
    TSUNIT_EQUAL(2, sf1.tablesCount());
    TSUNIT_EQUAL(ts::TID_PAT, sf1.tables()[0]->tableId());
    TSUNIT_EQUAL(ts::TID_PMT, sf1.tables()[1]->tableId());

    ts::ByteBlock output(3);
    TSUNIT_EQUAL(87, sf1.saveBuffer(output));
    TSUNIT_EQUAL(90, output.size());
    TSUNIT_EQUAL(0, ts::MemCompare(&output[3], psi_pat1_sections, sizeof(psi_pat1_sections)));
    TSUNIT_EQUAL(0, ts::MemCompare(&output[3 + 32], psi_pmt_scte35_sections, sizeof(psi_pmt_scte35_sections)));

    uint8_t out1[40];
    TSUNIT_EQUAL(32, sf1.saveBuffer(out1, sizeof(out1)));
    TSUNIT_EQUAL(0, ts::MemCompare(out1, psi_pat1_sections, sizeof(psi_pat1_sections)));

    uint8_t out2[100];
    TSUNIT_EQUAL(87, sf1.saveBuffer(out2, sizeof(out2)));
    TSUNIT_EQUAL(0, ts::MemCompare(out2, psi_pat1_sections, sizeof(psi_pat1_sections)));
    TSUNIT_EQUAL(0, ts::MemCompare(out2 + 32, psi_pmt_scte35_sections, sizeof(psi_pmt_scte35_sections)));
}

TSUNIT_DEFINE_TEST(Attribute)
{
    const ts::UChar* xmlref =
        u"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        u"<tsduck>\n"
        u"  <PAT version=\"0\" current=\"true\" transport_stream_id=\"0x0001\" network_PID=\"0x0010\">\n"
        u"    <metadata attribute=\"foo\"/>\n"
        u"    <service service_id=\"0x0100\" program_map_PID=\"0x0200\"/>\n"
        u"  </PAT>\n"
        u"</tsduck>\n";

    ts::DuckContext duck;
    ts::xml::Document doc1(report());

    TSUNIT_ASSERT(doc1.parse(xmlref));
    const ts::xml::Element* root1 = doc1.rootElement();
    TSUNIT_ASSERT(root1 != nullptr);
    TSUNIT_EQUAL(u"tsduck", root1->name());
    root1 = root1->firstChildElement();
    TSUNIT_ASSERT(root1 != nullptr);
    TSUNIT_EQUAL(u"PAT", root1->name());

    ts::BinaryTable table1;
    TSUNIT_ASSERT(table1.fromXML(duck, root1));
    TSUNIT_ASSERT(table1.isValid());
    TSUNIT_ASSERT(!table1.isShortSection());
    TSUNIT_EQUAL(ts::TID_PAT, table1.tableId());
    TSUNIT_EQUAL(u"foo", table1.attribute());

    TSUNIT_EQUAL(1, table1.sectionCount());
    TSUNIT_ASSERT(table1.sectionAt(0) != nullptr);
    TSUNIT_ASSERT(table1.sectionAt(0)->isValid());
    TSUNIT_EQUAL(u"foo", table1.sectionAt(0)->attribute());

    ts::PAT pat1(duck, table1);
    TSUNIT_ASSERT(pat1.isValid());
    TSUNIT_EQUAL(ts::TID_PAT, pat1.tableId());
    TSUNIT_EQUAL(1, pat1.ts_id);
    TSUNIT_EQUAL(ts::PID_NIT, pat1.nit_pid);
    TSUNIT_EQUAL(1, pat1.pmts.size());
    TSUNIT_EQUAL(0x100, pat1.pmts.begin()->first);
    TSUNIT_EQUAL(0x200, pat1.pmts.begin()->second);
    TSUNIT_EQUAL(u"foo", pat1.attribute());

    ts::BinaryTable table2;
    TSUNIT_ASSERT(pat1.serialize(duck, table2));
    TSUNIT_ASSERT(table2.isValid());
    TSUNIT_ASSERT(table2.fromXML(duck, root1));
    TSUNIT_ASSERT(!table2.isShortSection());
    TSUNIT_EQUAL(ts::TID_PAT, table2.tableId());
    TSUNIT_EQUAL(u"foo", table2.attribute());

    ts::PAT pat2;
    pat2 = pat1;
    TSUNIT_EQUAL(u"foo", pat1.attribute());
    TSUNIT_EQUAL(u"foo", pat2.attribute());

    ts::BinaryTable table3;
    table3 = table1;
    TSUNIT_EQUAL(u"foo", table1.attribute());
    TSUNIT_EQUAL(u"foo", table3.attribute());

    ts::xml::Document doc2(report());
    ts::xml::Element* root2 = doc2.initialize(u"tsduck", ts::xml::Declaration::DEFAULT_XML_DECLARATION);
    TSUNIT_ASSERT(root2 != nullptr);
    pat1.toXML(duck, root2);
    TSUNIT_EQUAL(xmlref, doc2.toString());

    ts::xml::Document doc3(report());
    ts::xml::Element* root3 = doc3.initialize(u"tsduck", ts::xml::Declaration::DEFAULT_XML_DECLARATION);
    TSUNIT_ASSERT(root3 != nullptr);
    table2.toXML(duck, root3);
    TSUNIT_EQUAL(xmlref, doc3.toString());
}
