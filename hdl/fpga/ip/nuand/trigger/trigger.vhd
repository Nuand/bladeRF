library ieee ;
    use ieee.std_logic_1164.all ;

entity trigger is
  generic (
    DEFAULT_OUTPUT  : std_logic := '0'
  );
  port (
    armed           :   in  std_logic;
    fired           :   in  std_logic;
    master          :   in  std_logic;
    trigger_in      :   in  std_logic;
    signal_in       :   in  std_logic;

    trigger_out     :   out std_logic;
    signal_out      :   out std_logic
  );
end entity;

architecture async of trigger is
    signal triggered              : std_logic := '0';
begin
    triggered <= not armed or not trigger_in;
    signal_out <= signal_in when triggered else DEFAULT_OUTPUT;
    trigger_out <= not fired when master else 'Z';
end architecture;
