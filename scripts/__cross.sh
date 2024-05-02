
echo "######## START"

echo $(pwd)

export OPENSSL_CONF="$(pwd)/scripts/openssl-ca.cnf"

export OPENSSL_MODULES="$(pwd)/_build/lib"

openssl list -providers

#openssl list -signature-algorithms -provider oqsprovider

echo "######## TEST"

./_build/test/oqs_test_signatures oqsprovider ${OPENSSL_CONF}

#./_build/test/oqs_test_endecode oqsprovider ${OPENSSL_CONF}

#./_build/test/oqs_test_tlssig oqsprovider ${OPENSSL_CONF} $(pwd)/tmp

echo "######## END"