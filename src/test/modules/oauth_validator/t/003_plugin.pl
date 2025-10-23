#
# Exercises the libpq plugin API for custom OAuth client flows.
#
# Copyright (c) 2025, PostgreSQL Global Development Group
#

use strict;
use warnings FATAL => 'all';

use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

if (!$ENV{PG_TEST_EXTRA} || $ENV{PG_TEST_EXTRA} !~ /\boauth\b/)
{
	plan skip_all =>
	  'Potentially unsafe test oauth not enabled in PG_TEST_EXTRA';
}

#
# Cluster Setup
#

my $node = PostgreSQL::Test::Cluster->new('primary');
$node->init;
$node->append_conf('postgresql.conf', "log_connections = all\n");
$node->append_conf('postgresql.conf',
	"oauth_validator_libraries = 'validator'\n");
$node->start;

$node->safe_psql('postgres', 'CREATE USER test;');

# These tests use a plugin flow that does not contact an authorization server,
# so the address used here shouldn't matter. Use an invalid IP address, so if
# there's some cascade of errors that causes the client to attempt a connection,
# we'll fail noisily.
my $issuer = "https://256.256.256.256";
my $scope = "openid postgres";

unlink($node->data_dir . '/pg_hba.conf');
$node->append_conf(
	'pg_hba.conf', qq{
local all test oauth issuer="$issuer" scope="$scope"
});
$node->reload;

my $log_start = $node->wait_for_log(qr/reloading configuration files/);

$ENV{PGOAUTHDEBUG} = "UNSAFE";

#
# Tests
#

my $user = "test";
my $base_connstr = $node->connstr() . " user=$user oauth_plugin=flow";
my $common_connstr =
  "$base_connstr oauth_issuer=$issuer oauth_client_id=myID";

$node->connect_ok(
	$common_connstr,
	"connect via plugin flow",
	log_like => [
		qr/oauth_validator: token="flowtoken", role="$user"/,
		qr/connection authenticated: identity="$user" method=oauth/,
	]);

done_testing();
