# Copyright (c) 2021-2025, PostgreSQL Global Development Group
use strict;
use warnings FATAL => 'all';

use PostgreSQL::Test::Utils;
use Test::More;
use IPC::Run;

my %result;

my $cmd = [ 'oauth_tests' ];
IPC::Run::run $cmd, '>', \$result{stdout}, '2>', \$result{stderr};
my $ret = $?;

chomp($result{stdout});
chomp($result{stderr});

SKIP:
{
	skip $result{stdout}, 1 if ($ret >> 8) == 2;

	if (!is($ret, 0, "oauth unit tests pass"))
	{
		is($result{stderr}, '', "oauth unit tests: stderr");
	}
}

done_testing();
