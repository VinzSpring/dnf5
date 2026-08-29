// Copyright Contributors to the DNF5 project.
// Copyright Contributors to the libdnf project.
// SPDX-License-Identifier: GPL-2.0-or-later
//
// This file is part of libdnf: https://github.com/rpm-software-management/libdnf/
//
// Libdnf is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Libdnf is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libdnf.  If not, see <https://www.gnu.org/licenses/>.


#include "test_package_sack.hpp"

#include "../shared/utils.hpp"

#include <libdnf5/repo/repo_query.hpp>
#include <libdnf5/rpm/package_query.hpp>
#include <libdnf5/rpm/package_sack.hpp>
#include <libdnf5/rpm/package_set.hpp>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <set>
#include <vector>


CPPUNIT_TEST_SUITE_REGISTRATION(RpmPackageSackTest);

using namespace libdnf5::rpm;

namespace {

// make constructor public so we can create Package instances in the tests
class TestPackage : public Package {
public:
    TestPackage(libdnf5::Base & base, PackageId id) : libdnf5::rpm::Package(base.get_weak_ptr(), id) {}
};

}  // namespace


void RpmPackageSackTest::setUp() {
    BaseTestCase::setUp();
    add_repo_solv("solv-24pkgs");

    pkgset = std::make_unique<PackageSet>(base);
    pkg0 = std::make_unique<TestPackage>(base, libdnf5::rpm::PackageId(0));
    pkgset->add(*pkg0);
}


void RpmPackageSackTest::test_set_user_excludes() {
    CPPUNIT_ASSERT(sack->get_user_excludes().size() == 0);
    sack->set_user_excludes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_excludes().contains(*pkg0) == true);
    CPPUNIT_ASSERT(sack->get_user_excludes().size() == 1);
    sack->clear_user_excludes();
    CPPUNIT_ASSERT(sack->get_user_excludes().size() == 0);
}


void RpmPackageSackTest::test_add_user_excludes() {
    CPPUNIT_ASSERT(sack->get_user_excludes().contains(*pkg0) == false);
    sack->add_user_excludes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_excludes().contains(*pkg0) == true);
    CPPUNIT_ASSERT(sack->get_user_excludes().size() == 1);

    // add the same package again
    sack->add_user_excludes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_excludes().size() == 1);
}


void RpmPackageSackTest::test_remove_user_excludes() {
    // remove package from empty excludes list does not fail
    CPPUNIT_ASSERT(sack->get_user_excludes().contains(*pkg0) == false);
    sack->remove_user_excludes(*pkgset);

    // remove existing package
    sack->set_user_excludes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_excludes().contains(*pkg0) == true);
    sack->remove_user_excludes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_excludes().contains(*pkg0) == false);
}


void RpmPackageSackTest::test_set_user_includes() {
    CPPUNIT_ASSERT(sack->get_user_includes().size() == 0);
    sack->set_user_includes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_includes().contains(*pkg0) == true);
    CPPUNIT_ASSERT(sack->get_user_includes().size() == 1);
    sack->clear_user_includes();
    CPPUNIT_ASSERT(sack->get_user_includes().size() == 0);
}


void RpmPackageSackTest::test_add_user_includes() {
    CPPUNIT_ASSERT(sack->get_user_includes().contains(*pkg0) == false);
    sack->add_user_includes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_includes().contains(*pkg0) == true);
    CPPUNIT_ASSERT(sack->get_user_includes().size() == 1);

    // add the same package again
    sack->add_user_includes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_includes().size() == 1);
}


void RpmPackageSackTest::test_remove_user_includes() {
    // remove package from empty includes list does not fail
    CPPUNIT_ASSERT(sack->get_user_includes().contains(*pkg0) == false);
    sack->remove_user_includes(*pkgset);

    // remove existing package
    sack->set_user_includes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_includes().contains(*pkg0) == true);
    sack->remove_user_includes(*pkgset);
    CPPUNIT_ASSERT(sack->get_user_includes().contains(*pkg0) == false);
}


void RpmPackageSackTest::test_uploaded_prior_to_excludes() {
    // solv-24pkgs has no baseurl; repomd-repo1 is the data with build times
    libdnf5::repo::RepoQuery fixture_repos(base);
    fixture_repos.filter_id("solv-24pkgs");
    for (const auto & fixture_repo : fixture_repos) {
        fixture_repo->disable();
    }

    auto repo = add_repo_repomd("repomd-repo1");

    auto & uploaded_prior_to = base.get_config().get_uploaded_prior_to_option();

    const auto repo_pkgs_count = [this](libdnf5::sack::ExcludeFlags flags) {
        PackageQuery query(base, flags);
        query.filter_repo_id("repomd-repo1");
        return query.size();
    };

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), repo_pkgs_count(libdnf5::sack::ExcludeFlags::APPLY_EXCLUDES));

    uploaded_prior_to.set("P7D");
    sack->load_config_excludes_includes();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), repo_pkgs_count(libdnf5::sack::ExcludeFlags::APPLY_EXCLUDES));

    uploaded_prior_to.set(std::numeric_limits<int32_t>::max());
    sack->load_config_excludes_includes();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), repo_pkgs_count(libdnf5::sack::ExcludeFlags::APPLY_EXCLUDES));

    CPPUNIT_ASSERT_EQUAL(
        static_cast<size_t>(3), repo_pkgs_count(libdnf5::sack::ExcludeFlags::IGNORE_REGULAR_CONFIG_EXCLUDES));

    base.get_config().get_disable_excludes_option().set("*");
    sack->load_config_excludes_includes();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), repo_pkgs_count(libdnf5::sack::ExcludeFlags::APPLY_EXCLUDES));
    base.get_config().get_disable_excludes_option().set(std::vector<std::string>{});

    uploaded_prior_to.set(std::numeric_limits<int32_t>::max());
    repo->get_config().get_uploaded_prior_to_option().set("0");
    sack->load_config_excludes_includes();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), repo_pkgs_count(libdnf5::sack::ExcludeFlags::APPLY_EXCLUDES));
}
