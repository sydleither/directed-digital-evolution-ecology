#define CATCH_CONFIG_MAIN

#include "Catch/single_include/catch2/catch.hpp"

#include <unordered_set>

#include "emp/datastructs/vector_utils.hpp"

#include "dirdevo/ExperimentSetups/AvidaGPL9/L9TaskSet.hpp"

TEST_CASE("L9 TaskSet", "[l9][TaskSet]")
{

  dirdevo::L9TaskSet task_set;

  CHECK(task_set.GetSize() == 10);
  CHECK(task_set.HasTask("ECHO"));
  CHECK(task_set.HasTask("NOT"));
  CHECK(task_set.HasTask("NAND"));
  CHECK(task_set.HasTask("OR_NOT"));
  CHECK(task_set.HasTask("AND"));
  CHECK(task_set.HasTask("OR"));
  CHECK(task_set.HasTask("AND_NOT"));
  CHECK(task_set.HasTask("NOR"));
  CHECK(task_set.HasTask("XOR"));
  CHECK(task_set.HasTask("EQU"));

  auto& echo_task = task_set.GetTask(task_set.GetID("ECHO"));
  CHECK(echo_task.calc_output_fun({0}) == 0);

}
