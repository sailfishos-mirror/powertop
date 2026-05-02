#include <iostream>
#include <vector>
#include <string>

#include "report/report-maker.h"
#include "report/report-formatter-md.h"
#include "report/report-data-html.h"
#include "test_framework.h"
#include "../test_helper.h"

void (*ui_notify_user)(const std::string &) = nullptr;

static void test_md_basic()
{
	report_maker report(REPORT_MD);
	
	report.add_logo();
	report.add_header();
	
	struct tag_attr title_attr;
	report.add_title(&title_attr, "Summary");
	
	std::vector<std::string> summary = {"Field1", "Value1", "Field2", "Value2"};
	report.add_summary_list(summary);
	
	struct table_attributes tb_attr;
	tb_attr.rows = 2;
	tb_attr.cols = 2;
	std::vector<std::string> table_data = {"Header1", "Header2", "Row1Col1", "Row1Col2"};
	report.add_table(table_data, &tb_attr);
	
	report.finish_report();
	std::string res = report.get_result();
	
	FILE *f = fopen("test.md", "w");
	if (f) {
		fprintf(f, "%s", res.c_str());
		fclose(f);
	}
	
	PT_ASSERT_TRUE(res.find("# PowerTOP Report") != std::string::npos);
	PT_ASSERT_TRUE(res.find("______________________________________________________________________") != std::string::npos);
	PT_ASSERT_TRUE(res.find("## Summary") != std::string::npos);
	PT_ASSERT_TRUE(res.find("- **Field1**: Value1") != std::string::npos);
	PT_ASSERT_TRUE(res.find("| Header1 | Header2 |") != std::string::npos);
	PT_ASSERT_TRUE(res.find("|---|---|") != std::string::npos);
	PT_ASSERT_TRUE(res.find("| Row1Col1 | Row1Col2 |") != std::string::npos);
}

int main()
{
	PT_RUN_TEST(test_md_basic);
	return pt_test_summary();
}
