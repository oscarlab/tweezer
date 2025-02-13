set(ycsbc_srcs
  ycsbc.cc
  db/db_factory.cc
  db/hashtable_db.cc
  db/rocks_db.cc
  core/core_workload.cc)

add_executable(ycsbc ${rados_srcs})

target_link_libraries(ycsbc ${ROCKSDB_LIB})

