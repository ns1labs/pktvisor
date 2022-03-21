Feature: pktvisor tests

Scenario: pktvisor bootstrap
  When run pktvisor on port default
  Then the pktvisor container status must be running
    And pktvisor API must be enabled

Scenario: run multiple pktvisors using different ports
  When run pktvisor on port default
    And run pktvisor on port 10854
    And run pktvisor on port 10855
  Then all the pktvisor containers must be running
    And 3 pktvisor's containers must be running

Scenario: run multiple pktvisors using the same port
  When run pktvisor on port default
    And run pktvisor on port default
  Then 1 pktvisor's containers must be running
    And 1 pktvisor's containers must be exited