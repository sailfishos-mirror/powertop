/* Copyright (C) 2026 Intel Corporation
 * Markdown report generator header.
 */
#pragma once
#include "report-formatter-base.h"

class report_formatter_md : public report_formatter_string_base
{
public:
	report_formatter_md();
	virtual void finish_report() override;
	virtual void add_logo() override;
	virtual void add_header() override;
	virtual void add_div(struct tag_attr *div_attr) override;
	virtual void add_title(struct tag_attr *title_att, const std::string &title) override;
	virtual void add_summary_list(const std::vector<std::string> &list) override;
	virtual void add_table(const std::vector<std::string> &system_data, struct table_attributes *tb_attr) override;

	virtual std::string get_result() override;

protected:
	virtual std::string escape_string(const std::string &str) override;
};
